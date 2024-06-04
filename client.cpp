#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <linux/net_tstamp.h>
#include <sys/poll.h>

#include "err.h"
#include "parser.h"
#include "client_communication.h"

class Message {
private:
    int type{};
    std::string value{};
    std::mutex mutex{};
    bool score_total = false;
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
    std::string get_message() noexcept {
        std::unique_lock<std::mutex> lock(mutex);
        return value;
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
    std::vector<std::vector<Card>> get_tricks() noexcept {
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

static Message last_msg;
static DealState game;
static int game_over = eventfd(0, 0);

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
            auto t = game.get_tricks();
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
    int enable = SOF_TIMESTAMPING_TX_SOFTWARE |
                 SOF_TIMESTAMPING_RX_SOFTWARE |
                 SOF_TIMESTAMPING_SOFTWARE;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_TIMESTAMPING, &enable, sizeof(int)) < 0) {
        close(socket_fd);
        syserr("setsockopt");
    }
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
            std::cerr << msg.first << ' ' << msg.second;
            if (msg.first == INCORRECT) {
                std::cerr << "Something went wrong\n";
                continue;
            }
            last_msg.update(msg);
            std::smatch matches;
            std::regex_match(msg.second, matches, regexes[msg.first]);
            std::stringstream ss;
            std::vector<Card> cards;
            switch (msg.first) {
                case BUSY:
                    increment_event_fd(game_over);
                    ss << "Place busy, list of busy places received: ";
                    for (size_t i = 1; i < matches.size() && !matches[i].str().empty(); i++) {
                        ss << matches[i];
                        if (i < matches.size() - 1 && !matches[i + 1].str().empty())
                            ss << ", ";
                    }
                    ss << '.';
                    if (!config.auto_player)
                        std::cout << ss.str() << std::endl;
                    throw std::runtime_error("");
                case DEAL:
                    ss << "New deal: " << matches[1] << ": staring place "
                       << matches[2] << ", your cards: ";
                    for (size_t i = 3; i < matches.size(); i++)
                        cards.emplace_back(matches[i]);
                    ss << print_list(cards) << '.';
                    game.set_hand(cards);
                    break;
                case TRICK:
                    ss << "Trick: (" << matches[1] << ") ";
                    for (size_t i = 2; i < matches.size() && !matches[i].str().empty(); i++)
                        cards.emplace_back(matches[i]);
                    ss << print_list(cards) << "\nAvailable: "
                       << print_list(game.get_hand());
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
                    for (size_t i = 2; i < matches.size() - 1; i++)
                        cards.emplace_back(matches[i]);
                    game.put_trick(cards, (matches[matches.size() - 1] == config.seat));
                    ss << print_list(cards);
                    break;
                case SCORE:
                    ss << "The scores are:\n";
                    for (int i = 0; i < 4; i++) {
                        ss << matches[1 + 2 * i] << " | " << matches[2 + 2 * i];
                        if (i < 3)
                            ss << '\n';
                    }
                    break;
                case TOTAL:
                    ss << "The total scores are:\n";
                    for (int i = 0; i < 4; i++) {
                        ss << matches[1 + 2 * i] << " | " << matches[2 + 2 * i];
                        if (i < 3)
                            ss << '\n';
                    }
                    break;
            }
            std::cerr << "After dealing with msg\n";
            if (!config.auto_player)
                std::cout << ss.str() << std::endl;
            //else
            //    std::cerr << ss.str() << std::endl;
        }
    }
    catch (const std::runtime_error &e) {
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
        if (!last_msg.may_end()) {
            if (!std::string(e.what()).empty())
                error(e.what());
            return 1;
        }
    }
    return 0;
}