#include "server_players.h"

#include <atomic>
#include <bitset>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <regex>
#include <thread>
#include <unistd.h>

#include "common.h"
#include "err.h"
#include "server_inside.h"
#include "card.h"

class AtomicMap {
private:
    std::mutex mutex;
    std::bitset<4> active{0};
    std::atomic_flag game_over = ATOMIC_FLAG_INIT;
public:
    [[nodiscard]] std::string setActive (char c) {
        if (game_over.test())
            return "NESW";
        std::string ans;
        mutex.lock();
        int pos = get_index_from_seat(c);
        if (active.test(pos)) {
            for (int i = 0; i < 4; i++)
                if (active.test(i))
                    ans += get_seat_from_index(i);
        }
        else
            active.set(pos);
        mutex.unlock();
        return ans;
    }

    void disconnect (const int &pos) {
        mutex.lock();
        active.reset(pos);
        mutex.unlock();
    }

    void end_game () {
        game_over.test_and_set();
    }
};

static AtomicMap active;

char recv_IAM (const int &client_fd) {
    static constexpr ssize_t IAM_SIZE = 6;
    static const std::regex IAM_REGEX("IAM[NESW]\r\n");
    char buf[IAM_SIZE + 1] = {0};
    if (readn(client_fd, buf, IAM_SIZE) < IAM_SIZE) {
        error("receiving IAM");
        throw std::runtime_error("");
    }
    if (!std::regex_match(std::string(buf, IAM_SIZE), IAM_REGEX)) {
        error("Invalid IAM message %s", buf);
        throw std::runtime_error("");
    }
    return buf[3];
}

void send_BUSY (const int &client_fd, const std::string &s) {
    const std::string to_send = "BUSY" + s + "\r\n";
    if (writen(client_fd, to_send.c_str(), to_send.size()) < static_cast<ssize_t>(to_send.size()))
        error("sending BUSY");
}

void send_disconnect (const int &gm_pipe, const int &pos) {
    char c = DISCONNECT;
    if (write(gm_pipe, &c, 1) < 0)
        syserr ("writen");
    active.disconnect(pos);
}

std::vector<Card> process_hand (const int &gm_pipe, const int &client_fd) {
    // TODO: read line
    char tmp[100];
    ssize_t v = read(gm_pipe, tmp, sizeof tmp);
    if (v < 0)
        syserr("read from pipe");
    std::string rcv(tmp, v);
    // first 6 positions: DEAL<deal><starter>, last two: \r\n
    std::vector<Card> ans = parse_cards(rcv.substr(6, rcv.size() - 8));
    if (writen(client_fd, tmp, v) < 0)
        syserr("sending DEAL");
    return ans;
}

void handle_player(
        const int &client_fd,
        const sockaddr_in6 &client_address,
        const std::array<int, 4> &to_gm,
        const std::array<int, 4> &from_gm,
        const int &timeout
) {
    timeval to = {.tv_sec = timeout, .tv_usec = 0};
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof to);
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof to);
    char seat;
    try {
        seat = recv_IAM(client_fd);
        std::string ans = active.setActive(seat);
        if (!ans.empty()) {
            std::cerr << "BUSY\n";
            send_BUSY(client_fd, ans);
            close(client_fd);
            return;
        }
    }
    catch (const std::runtime_error &) {
        close(client_fd);
        return;
    }

    int pos = get_index_from_seat(seat);
    int to_gm_fd = to_gm[pos];
    int from_gm_fd = from_gm[pos];

    char c = CONNECT;
    if (writen(to_gm_fd, &c, 1) < 0)
        syserr ("writen");
    std::vector<Card> hand = process_hand(from_gm_fd, client_fd);
    /*std::cerr << "HAND: ";
    for (auto card : hand)
        std::cerr << card.to_string() << ' ';
    std::cerr << '\n';*/

    sleep(1);
    send_disconnect(to_gm_fd, pos);

    close(client_fd);
}