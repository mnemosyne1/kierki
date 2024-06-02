#include "server_players.h"

#include <algorithm>
#include <bitset>
#include <iostream>
#include <poll.h>
#include <regex>
#include <unistd.h>
#include <sys/eventfd.h>

#include "common.h"
#include "err.h"
#include "card.h"
#include "server_inside.h"

// GLOBAL VARIABLES

static ActiveMap active;
static GameState game;
// incremented when: (a) game is paused (b) game is unpaused (c) it's player's turn
const int to_pl[4] = {eventfd(0, EFD_SEMAPHORE), eventfd(0, EFD_SEMAPHORE),
                         eventfd(0, EFD_SEMAPHORE), eventfd(0, EFD_SEMAPHORE)};
const int pl_to_gm = eventfd(0, EFD_SEMAPHORE);
bool game_paused = true;

// AUXILIARY FUNCTIONS
// SENDS/RECEIVES

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

void send_BUSY(SendData &send_data, const std::string &occupied) {
    const std::string s = "BUSY" + occupied + "\r\n";
    if (writen(send_data, s.c_str(), s.size()) < static_cast<ssize_t>(s.size()))
        throw std::runtime_error("sending BUSY");
}

void send_DEAL(SendData &send_data, const std::vector<Card> &hand) {
    std::string s = "DEAL" + std::to_string(game.get_deal()) +
        game.get_first() + cards_to_string(hand) + "\r\n";
    if (writen(send_data, s.c_str(), s.size()) < static_cast<ssize_t>(s.size()))
        throw std::runtime_error("sending DEAL");
}

void send_WRONG(SendData &send_data, int trick) {
    std::string s = "WRONG" + std::to_string(trick) + "\r\n";
    if (writen(send_data, s.c_str(), s.size()) < static_cast<ssize_t>(s.size()))
        throw std::runtime_error("sending WRONG");
}

void send_TAKEN(SendData &send_data, int trick) {
    std::string s = game.get_TAKEN(trick);
    if (writen(send_data, s.c_str(), s.size()) < static_cast<ssize_t>(s.size()))
        throw std::runtime_error("sending TRICK");
}

void send_SCORE(SendData &send_data) {
    std::string s = game.get_SCORE();
    if (writen(send_data, s.c_str(), s.size()) < static_cast<ssize_t>(s.size()))
        throw std::runtime_error("sending SCORE");
    s = game.get_TOTAL();
    if (writen(send_data, s.c_str(), s.size()) < static_cast<ssize_t>(s.size()))
        throw std::runtime_error("sending TOTAL");
}

// OTHER FUNCTIONS

void process_hand (SendData &send_data, const int &pos, std::vector <Card> &hand) {
    if (to_pl[pos] == -1)
        throw std::runtime_error("initialising eventfd");
    std::cerr << "Processing " << pos << '\n';
    pollfd fds[2];
    fds[0] = {.fd = to_pl[pos], .events = POLLIN, .revents = 0};
    fds[1] = {.fd = send_data.get_fd(), .events = POLLIN, .revents = 0};
    poll(fds, 2, -1);
    // on client_fd we'd get either disconnect or unwanted messages
    if (fds[1].revents & POLLIN) {
        increment_event_fd(to_pl[pos]);
        std::cerr << "Incremented\n";
        throw std::runtime_error("got from client");
    }
    // else we got a message on event_fd
    decrement_event_fd(to_pl[pos]);
    hand = game.get_hand(pos);
    send_DEAL(send_data, hand);
    int i = 1;
    while (game.get_trick(i).size() == 4)
        send_TAKEN(send_data, i++);
}

void wait_for_turn (int pos, int client_fd) {
    pollfd fds[2];
    while (true) {
        fds[0] = {.fd = to_pl[pos], .events = POLLIN, .revents = 0};
        fds[1] = {.fd = client_fd, .events = POLLIN, .revents = 0};
        poll(fds, 2, -1);
        // on client_fd we'd get either disconnect or unwanted messages
        if (fds[1].revents & POLLIN)
            throw std::runtime_error("got from client");
        // else we got a message on event_fd
        decrement_event_fd(to_pl[pos]);
        if (!game_paused)
            return;
        else
            decrement_event_fd(to_pl[pos]); // wait for unpause
    }
}

bool incorrect_color(const std::vector<Card> &hand, const std::vector<Card> &trick, const Card &c) {
    if (trick.empty() || trick[0].get_suit() == c.get_suit())
        return false;
    return std::ranges::any_of(hand, [&trick](const Card &c){return c.get_suit() == trick[0].get_suit();});
}

// ACTUAL THREAD FUNCTIONS

void handle_player(
        const int &client_fd,
        const sockaddr_storage &client_address,
        const sockaddr_storage &server_address,
        const int &timeout,
        Log &log
) {
    timeval to = {.tv_sec = timeout, .tv_usec = 0};
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof to);
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof to);
    char seat;
    int pos = 0;
    bool connected = false;
    SendData send_data(client_fd, server_address, client_address);
    try {
        seat = get_IAM(send_data);
        std::string ans = active.setActive(seat);
        if (!ans.empty()) {
            send_BUSY(send_data, ans);
            close(client_fd);
            return;
        }
        pos = get_index_from_seat(seat);
        connected = true;
        decrement_event_fd(to_pl[pos]); // acknowledge game is paused
        std::vector<Card> hand;
        while (!active.is_over()) {
            process_hand(send_data, pos, hand);
            int trick_no = game.get_trick_no();
            while (trick_no <= 13) {
                wait_for_turn(pos, client_fd);
                const auto &trick = game.get_trick(trick_no);
                send_TRICK(send_data, trick_no, trick);
                while (true) {
                    try {
                        auto [no, v] = get_TRICK<false>(send_data); // TODO: beware of disconnects
                        if (v.size() != 1)
                            throw std::runtime_error("Incorrect answer to TRICK (cards no. >1)");
                        if (no != trick_no || incorrect_color(hand, trick, v[0]) || std::erase(hand, v[0]) == 0) {
                            send_WRONG(send_data, trick_no);
                            continue;
                        }
                        game.play(v[0]);
                        break;
                    }
                    catch (const std::runtime_error &e) {
                        if (e.what() == timeout_trick_msg)
                            send_TRICK(send_data, trick_no, trick);
                        else
                            throw e;
                    }
                }
                if (trick.size() == 4)
                    increment_event_fd(pl_to_gm);
                else
                    increment_event_fd(to_pl[(pos + 1) % 4]);
                wait_for_turn(pos, client_fd); // waiting for end of trick
                send_TAKEN(send_data, trick_no);
                trick_no++;
            }
            send_SCORE(send_data);
            increment_event_fd(pl_to_gm);
            decrement_event_fd(to_pl[pos]);
        }
    }
    catch (const std::runtime_error &e) {
        error(e.what());
        if (connected)
            active.disconnect(pos, pl_to_gm);
        close(client_fd);
        send_data.append_to_log(log);
        return;
    }
    active.disconnect(pos, pl_to_gm);
    send_data.append_to_log(log);

    close(client_fd);
}

void game_master(std::ifstream &desc, const int &game_over_fd) {
    for (const int &fd: to_pl)
        increment_event_fd(fd); // start a (paused) game
    int current_deal;
    char starting_client;
    while (get_deal(desc, current_deal, starting_client)) {
        active.wait_for_four();
        game_paused = false;
        std::cerr << "four\n";
        clear_event_fd(pl_to_gm);
        for (const auto &fd: to_pl)
            increment_event_fd(fd); // let players know game has started
        int tricks = 0;
        while (tricks < 13) {
            increment_event_fd(to_pl[game.get_pos()]);
            while (true) {
                decrement_event_fd(pl_to_gm); // trick or disconnect
                if (active.is_four()) {
                    tricks++;
                    for (const auto &fd: to_pl)
                        increment_event_fd(fd); // let players know trick is over
                    break;
                }
                else { // TODO: nie działa
                    std::cerr << "Paused\n";
                    game_paused = true;
                    for (const int &fd: to_pl)
                        increment_event_fd(fd); // game paused
                    active.wait_for_four();
                    game_paused = false;
                    for (const int &fd: to_pl)
                        increment_event_fd(fd); // let players know game is back on
                }
            }
        }
        decrement_event_fd(pl_to_gm, 4);
        if (desc.peek() == EOF)
            active.end_game();
        else
            std::cerr << desc.peek();
        for (const int &fd: to_pl)
            increment_event_fd(fd);
    }
    // end the game
    increment_event_fd(game_over_fd);
}