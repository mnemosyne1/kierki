#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <regex>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netdb.h>

#include "common.h"
#include "err.h"
#include "parser.h"

class Message {
private:
    int type{};
    std::string value{};
    std::mutex mutex{};
    bool score_total = false; // were the last 2 messages score and total (in any order)
public:
    Message() = default;
    void update(const std::pair<int, std::string> &val) noexcept {
        std::unique_lock<std::mutex> lock(mutex);
        if ((type == SCORE && val.first == TOTAL) ||
            (type == TOTAL && val.first == SCORE))
            score_total = true;
        else
            score_total = false;
        type = val.first;
        value = val.second;
    }
    int get_type() noexcept {
        std::unique_lock<std::mutex> lock(mutex);
        return type;
    }
    bool may_end() noexcept {
        std::unique_lock<std::mutex> lock(mutex);
        return score_total;
    }
};
class DealState {
private:
    std::vector<Card> hand;
    std::vector<std::vector<Card>> tricks;
    int trick_no = 1;
    std::mutex mutex{};
public:
    DealState() = default;
    const std::vector<std::vector<Card>> &get_tricks() noexcept {
        std::unique_lock<std::mutex> lock(mutex);
        return tricks;
    }
    std::vector<Card> get_hand() noexcept {
        std::unique_lock<std::mutex> lock(mutex);
        return hand;
    }
    int get_trick() noexcept {
        std::unique_lock<std::mutex> lock(mutex);
        return trick_no;
    }
    void put_trick(const std::vector<Card> &cards, bool add) noexcept {
        std::unique_lock<std::mutex> lock(mutex);
        if (add)
            tricks.push_back(cards);
        trick_no++;
        for (const Card &c: cards) {
            auto f = std::find(hand.begin(), hand.end(), c);
            if (f != hand.end()) {
                hand.erase(f);
                return;
            }
        }
    }
    void set_hand(const std::vector<Card> &cards) noexcept {
        std::unique_lock<std::mutex> lock(mutex);
        hand = cards;
        trick_no = 1;
        tricks.clear();
    }
    Card get_playable(const std::vector<Card> &cards) noexcept {
        if (cards.empty())
            return hand[0];
        Suit s = cards[0].get_suit();
        for (const Card &c: hand)
            if (c.get_suit() == s)
                return c;
        return hand[0];
    }
};

// GLOBAL VARIABLES (+INTERTHREAD COMMUNICATION)

static Message last_msg;
static DealState game;
static int game_over = eventfd(0, 0);

[[nodiscard]] std::string print_list(const std::vector<Card> &cards) {
    ssize_t len = static_cast<ssize_t>(cards.size()) - 1;
    std::stringstream ss;
    for (ssize_t i = 0; i < len; i++)
        ss << cards[i].to_string() << ", ";
    if (len >= 0)
        ss << cards[len].to_string();
    return ss.str();
}

// COMMUNICATION

int socket_init(const std::string& host, const std::string &port,
                const int &ipv, sockaddr_storage *server_address) {
    addrinfo hints, *server_info;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = ipv;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &server_info) != 0)
        syserr("cannot get server info");
    int socket_fd;
    socket_fd = socket(server_info->ai_family,
                       server_info->ai_socktype,
                       server_info->ai_protocol);
    if (socket_fd < 0)
        syserr("cannot create a socket");
    if (connect(socket_fd, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        close(socket_fd);
        syserr("cannot connect to server");
    }
    memcpy(server_address, server_info->ai_addr, server_info->ai_addrlen);
    freeaddrinfo(server_info);
    return socket_fd;
}

void send_IAM(SendData &send_data, const char &seat) {
    static constexpr ssize_t IAM_LEN = 6;
    const char msg[] = {'I', 'A', 'M', seat, '\r', '\n'};
    if (writen(send_data, msg, IAM_LEN) != IAM_LEN)
        throw std::runtime_error("sending IAM");
}

// returns pair (type of message, message content)
std::pair<int, std::string> get_message(SendData &send_data) {
    std::pair<int, std::string> ans{INCORRECT, ""};
    auto tmp = get_line(send_data, ans.second);
    if (tmp <= 0)
        throw std::runtime_error("couldn't receive message");
    for (int i = 0; i < REGEXES_NO; i++) {
        if (std::regex_match(ans.second, regexes[i])) {
            ans.first = i;
            return ans;
        }
    }
    return ans;
}

std::vector<Card> process_card_message(std::stringstream &output,
                                       const std::smatch &matches,
                                       size_t first_card,
                                       size_t last_card) {
    std::vector<Card> cards;
    for (size_t i = first_card; i < last_card && !matches[i].str().empty(); i++)
        cards.emplace_back(matches[i]);
    output << print_list(cards);
    return cards;
}

void process_BUSY(std::stringstream &output, const std::smatch &matches) {
    increment_event_fd(game_over);
    output << "Place busy, list of busy places received: ";
    for (size_t i = 1; i < matches.size() && !matches[i].str().empty(); i++) {
        output << matches[i];
        if (i < matches.size() - 1 && !matches[i + 1].str().empty())
            output << ", ";
    }
    output << '.';
}

void process_score_message(std::stringstream &output, const std::smatch &matches) {
    for (int i = 0; i < 4; i++) {
        output << matches[1 + 2 * i] << " | " << matches[2 + 2 * i];
        if (i < 3)
            output << '\n';
    }
}

// THREAD FUNCTION

void cin_worker(SendData &send_data) {
    pollfd fds[2];
    std::string request;
    while (true) {
        fds[0] = {.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
        fds[1] = {.fd = game_over, .events = POLLIN, .revents = 0};
        poll(fds, 2, -1);
        if (fds[1].revents & POLLIN)
            return;
        // else we have something on stdin
        std::getline(std::cin, request);
        if (request == "cards") {
            std::cout << print_list(game.get_hand()) << std::endl;
        }
        else if (request == "tricks") {
            const auto &t = game.get_tricks();
            for (const auto &trick: t)
                std::cout << print_list(trick) << std::endl;
        }
        else if (request.starts_with('!')) {
            try {
                int msg = last_msg.get_type();
                if (msg != TRICK && msg != WRONG)
                    throw std::invalid_argument("");
                Card c(request.substr(1, request.size() - 1)); // may throw
                send_TRICK(send_data, game.get_trick(), std::vector<Card>{c});
            }
            catch (const std::invalid_argument&) {
                std::cout << "Invalid request\n";
            }
        }
        else
            std::cout << "Invalid request\n";
    }
}

int main(int argc, char *argv[]) {
    client_config config = get_client_config(argc, argv);
    sockaddr_storage server_address{}, client_address{};
    int socket_fd = socket_init(config.host, config.port, config.ipv, &server_address);
    auto addr_size = static_cast<socklen_t>(sizeof client_address);
    if (getsockname(socket_fd, (sockaddr *) &client_address, &addr_size)) {
        close(socket_fd);
        syserr("getsockname");
    }
    SendData send_data(socket_fd, client_address, server_address);
    std::thread cinner;
    if (!config.auto_player)
        cinner = std::thread(cin_worker, std::ref(send_data));
    try {
        send_IAM(send_data, config.seat);
        while (true) {
            auto msg = get_message(send_data);
            if (msg.first == INCORRECT)
                continue;
            last_msg.update(msg);
            std::smatch matches;
            std::regex_match(msg.second, matches, regexes[msg.first]);
            std::stringstream ss;
            std::vector<Card> cards;
            switch (msg.first) {
                case BUSY:
                    process_BUSY(ss, matches);
                    if (!config.auto_player)
                        std::cout << ss.str() << std::endl;
                    throw std::runtime_error("");
                case DEAL:
                    ss << "New deal: " << matches[1] << ": staring place "
                       << matches[2] << ", your cards: ";
                    cards = process_card_message(ss, matches, 3, matches.size());
                    ss << '.';
                    game.set_hand(cards);
                    break;
                case TRICK:
                    ss << "Trick: (" << matches[1] << ") ";
                    cards = process_card_message(ss, matches, 2, matches.size());
                    ss << "\nAvailable: " << print_list(game.get_hand());
                    if (config.auto_player) {
                        Card c = game.get_playable(cards);
                        send_TRICK(send_data, game.get_trick(), std::vector<Card>{c});
                    }
                    break;
                case WRONG:
                    ss << "Wrong message received in trick " << matches[1] << '.';
                    break;
                case TAKEN:
                    ss << "A trick " << matches[1] << " is taken by "
                       << matches[matches.size() - 1] << ", cards ";
                    cards = process_card_message(ss, matches, 2, matches.size() - 1);
                    game.put_trick(cards, (matches[matches.size() - 1] == config.seat));
                    ss << '.';
                    break;
                case SCORE:
                    ss << "The scores are:\n";
                    process_score_message(ss, matches);
                    break;
                case TOTAL:
                    ss << "The total scores are:\n";
                    process_score_message(ss, matches);
                    break;
            }
            if (!config.auto_player)
                std::cout << ss.str() << std::endl;
        }
    }
    catch (const std::runtime_error &e) {
        // server disconnected or other error occured
        increment_event_fd(game_over);
        if (config.auto_player) {
            Log log;
            send_data.append_to_log(log);
            for (const Log_message &msg: log)
                std::cout << msg.second;
        }
        else
            cinner.join();
        close(socket_fd);
        close(game_over);
        if (!last_msg.may_end()) {
            if (!std::string(e.what()).empty())
                error(e.what());
            return 1;
        }
    }
    return 0;
}