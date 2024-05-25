#include "server_players.h"

#include <bitset>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <regex>
#include <unistd.h>
#include <sys/eventfd.h>

#include "common.h"
#include "err.h"
#include "card.h"
#include "server_inside.h"

// GLOBAL FUNCTIONS

static ActiveMap active;
static GameState game;
const int gm_to_pl = eventfd(0, EFD_SEMAPHORE);
const int pl_to_gm = eventfd(0, EFD_SEMAPHORE);

// AUXILIARY FUNCTIONS

bool get_deal(std::ifstream &desc, int &current_deal, char &starting_client) {
    if (desc.eof())
        return false;
    static std::string tmp;
    std::getline(desc, tmp);
    if (desc.eof())
        return false;
    current_deal = tmp[0] - '0';
    starting_client = tmp[1];
    for (int i = 0; i < 4; i++) {
        std::getline(desc, tmp);
        game.start_game(i, parse_cards(tmp), current_deal, starting_client);
    }
    return true;
}

char recv_IAM (SendData &send_data) {
    static constexpr ssize_t IAM_SIZE = 6;
    static const std::regex IAM_REGEX("IAM[NESW]\r\n");
    char buf[IAM_SIZE + 1] = {0};
    if (readn(send_data, buf, IAM_SIZE) < IAM_SIZE)
        throw std::runtime_error("receiving IAM");
    std::string msg(buf, IAM_SIZE);
    //log_message(msg, client_address, server_address);
    if (!std::regex_match(msg, IAM_REGEX))
        throw std::runtime_error("Invalid IAM message");
    return buf[3];
}

void send_BUSY (SendData &send_data, const std::string &s) {
    const std::string to_send = "BUSY" + s + "\r\n";
    if (writen(send_data, to_send.c_str(), to_send.size()) < static_cast<ssize_t>(to_send.size()))
        error("sending BUSY");
    //log_message(to_send, server_address, client_address);
}

void send_DEAL(SendData &send_data, const std::vector<Card> &hand) {
    std::string s = "DEAL" + std::to_string(game.get_deal()) +
        game.get_next_player() + cards_to_string(hand) + "\r\n";
    if (writen(send_data, s.c_str(), s.size()) < 0)
        syserr("sending to client");
    //log_message(s, server_address, client_address);
}

void process_hand (SendData &send_data, const int &pos, std::vector <Card> &hand) {
    if (gm_to_pl == -1)
        throw std::runtime_error("initialising eventfd");
    pollfd fds[2];
    fds[0] = {.fd = gm_to_pl, .events = POLLIN, .revents = 0};
    fds[1] = {.fd = send_data.get_fd(), .events = POLLIN, .revents = 0};
    poll(fds, 2, -1);
    // on client_fd we'd get either disconnect or unwanted messages
    if (fds[1].revents & POLLIN)
        throw std::runtime_error("got from client");
    // else we got a message on event_fd
    decrement_event_fd(gm_to_pl);
    hand = game.get_hand(pos);
    send_DEAL(send_data, hand);
}

void wait_for_turn (char seat, int client_fd) {
    while (game.get_next_player() != seat) {
        pollfd fds[2];
        fds[0] = {.fd = gm_to_pl, .events = POLLIN, .revents = 0};
        fds[1] = {.fd = client_fd, .events = POLLIN, .revents = 0};
        poll(fds, 2, -1);
        // on client_fd we'd get either disconnect or unwanted messages
        if (fds[1].revents & POLLIN)
            throw std::runtime_error("got from client");
        // else we got a message on event_fd
        decrement_event_fd(gm_to_pl);
    }
}

// ACTUAL THREAD FUNCTIONS

void handle_player(
        const int &client_fd,
        const sockaddr_storage &client_address,
        const sockaddr_storage &server_address,
        const int &timeout
) {
    timeval to = {.tv_sec = timeout, .tv_usec = 0};
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof to);
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof to);
    char seat;
    int pos = 0;
    bool connected = false;
    SendData send_data(client_fd, server_address, client_address);
    try {
        seat = recv_IAM(send_data);
        std::string ans = active.setActive(seat);
        if (!ans.empty()) {
            send_BUSY(send_data, ans);
            throw std::runtime_error("Busy\n");
        }
        pos = get_index_from_seat(seat);
        connected = true;
        std::vector<Card> hand;
        process_hand(send_data, pos, hand);;
        send_data.print_log();
        wait_for_turn(seat, pos);
        std::cerr << "My turn!\n";
    }
    catch (const std::runtime_error &e) {
        error(e.what());
        if (connected)
            active.disconnect(pos, pl_to_gm);
        close(client_fd);
        return;
    }

    sleep(1);
    active.disconnect(pos, pl_to_gm);

    close(client_fd);
}

void game_master(std::ifstream &desc, const int &game_over_fd) {
    int current_deal;
    char starting_client;
    while (get_deal(desc, current_deal, starting_client)) {
        active.wait_for_four();
        increment_event_fd(gm_to_pl, 4); // let players know game has started
    }
    // end the game
    uint64_t inc = 1;
    write(game_over_fd, &inc, sizeof inc);
}