#include "server_threads.h"

#include <algorithm>
#include <iostream>
#include <poll.h>
#include <regex>
#include <sys/eventfd.h>

#include "common.h"
#include "err.h"
#include "card.h"
#include "server_classes.h"

// GLOBAL VARIABLES

static ActiveMap active;
static GameState game;
// pipes, message when: (a) game is paused (b) game is unpaused
// (c) it's player's turn (d) trick is over (time to send TAKEN)
int to_pl[4][2];
// increment when: (a) player disconnects (b) trick is over
// (c) deal is over, so we need to sync threads
const int pl_to_gm = eventfd(0, EFD_SEMAPHORE);

// defines of pipe messages
constexpr char PAUSE = 'P';
constexpr char PLAY = 'C';
constexpr char TURN = 'T';
constexpr char TAKE = 'O';

// AUXILIARY FUNCTIONS

char get_from_pipe (int pipe) {
    char msg;
    read(pipe, &msg, 1);
    return msg;
}

// SENDS/RECEIVES

bool get_deal(std::ifstream &desc, int &current_deal, char &starting_client) {
    if (desc.eof())
        return false;
    static std::string tmp;
    std::getline(desc, tmp);
    current_deal = tmp[0] - '0';
    starting_client = tmp[1];
    for (int i = 0; i < 4; i++) {
        std::getline(desc, tmp);
        game.start_game(i, parse_cards(tmp), current_deal, starting_client);
    }
    return true;
}

bool send_msg(SendData &send_data, const char *msg, size_t len) {
    return (writen(send_data, msg, len) == static_cast<ssize_t>(len));
}

void send_BUSY(SendData &send_data, const std::string &occupied) {
    const std::string s = "BUSY" + occupied + "\r\n";
    if (!send_msg(send_data, s.c_str(), s.size()))
        throw std::runtime_error("sending BUSY");
}

void send_DEAL(SendData &send_data, const std::vector<Card> &hand) {
    std::string s = "DEAL" + std::to_string(game.get_deal()) +
        game.get_first() + cards_to_string(hand) + "\r\n";
    if (!send_msg(send_data, s.c_str(), s.size()))
        throw std::runtime_error("sending DEAL");
}

void send_WRONG(SendData &send_data, int trick) {
    std::string s = "WRONG" + std::to_string(trick) + "\r\n";
    if (!send_msg(send_data, s.c_str(), s.size()))
        throw std::runtime_error("sending WRONG");
}

void send_TAKEN(SendData &send_data, int trick) {
    std::string s = game.get_TAKEN(trick);
    if (!send_msg(send_data, s.c_str(), s.size()))
        throw std::runtime_error("sending TRICK");
}

void send_SCORE(SendData &send_data) {
    std::string s = game.get_SCORE();
    if (!send_msg(send_data, s.c_str(), s.size()))
        throw std::runtime_error("sending SCORE");
    s = game.get_TOTAL();
    if (!send_msg(send_data, s.c_str(), s.size()))
        throw std::runtime_error("sending TOTAL");
}

std::pair<int, std::vector<Card>> get_TRICK(SendData &send_data, int pos, int timeout) {
    std::string trick;
    std::smatch matches;
    pollfd fds[2];
    bool waiting_for_unpause = false;
    while (true) {
        fds[0] = {.fd = to_pl[pos][0], .events = POLLIN, .revents = 0};
        fds[1] = {.fd = send_data.get_fd(), .events = static_cast<short>(waiting_for_unpause ? POLLRDHUP : POLLIN), .revents = 0};
        if (poll(fds, 2, waiting_for_unpause ? -1 : timeout) == 0) // timeout
            throw std::runtime_error(timeout_trick_msg);
        if (fds[0].revents & POLLIN) {
            char msg = get_from_pipe(to_pl[pos][0]);
            switch (msg) {
                case PAUSE:
                    waiting_for_unpause = true;
                    break;
                case PLAY:
                    waiting_for_unpause = false;
                    break;
                default: // should not happen
                    continue;
            }
        }
        // client disconnected
        if (fds[1].revents & POLLRDHUP) {
            if (waiting_for_unpause)
                write(to_pl[pos][1], &PAUSE, 1);
            write(to_pl[pos][1], &TURN, 1);
            throw std::runtime_error("client disconnected");
        }
        // we got a trick (or client disconnected)
        if (fds[1].revents & POLLIN)
            break;
    }
    if (get_line(send_data, trick) <= 0) { // known issue: doesn't check if game is paused here
        if (errno == EAGAIN)
            throw std::runtime_error(timeout_trick_msg);
        else {
            write(to_pl[pos][1], &TURN, 1);
            throw std::runtime_error("couldn't receive TRICK");
        }
    }
    if (std::regex_match(trick, matches, TRICK_REGEX)) {
        std::vector<Card> cards;
        for (size_t i = 2; i < matches.size() && !matches[i].str().empty(); i++)
            cards.emplace_back(matches[i].str());
        return {std::stoi(matches[1].str()), cards};
    }
    else {
        write(to_pl[pos][1], &TURN, 1);
        throw std::runtime_error("invalid TRICK: " + trick);
    }
}

// OTHER FUNCTIONS

void wait_for_deal (SendData &send_data, const int &pos, std::vector <Card> &hand) {
    pollfd fds[2];
    char msg = PAUSE;
    while (true) {
        fds[0] = {.fd = to_pl[pos][0], .events = POLLIN, .revents = 0};
        fds[1] = {.fd = send_data.get_fd(), .events = POLLIN, .revents = 0};
        poll(fds, 2, -1);
        // we got a message from gm: maybe unpause?
        if (fds[0].revents & POLLIN) {
            if (msg != PAUSE)
                write(to_pl[pos][1], &msg, 1);
            msg = get_from_pipe(to_pl[pos][0]);
            if (msg == PLAY)
                break;
            else
                continue;
        }
        // on client_fd we'd get either disconnect or unwanted messages
        // we can even disband TRICK here since the game hasn't started
        if (fds[1].revents & POLLIN) {
            write(to_pl[pos][1], &PAUSE, 1);
            throw std::runtime_error("client sent unexpected message or disconnected");
        }
    }
    hand = game.get_hand(pos);
    send_DEAL(send_data, hand);
    int i = 1;
    while (game.get_trick(i).size() == 4) {
        send_TAKEN(send_data, i);
        const auto &trick = game.get_trick(i);
        for (const Card &c: trick)
            if (std::find(hand.begin(), hand.end(), c) != hand.end())
                std::erase(hand, c);
        i++;
    }
}

char wait_for_turn (SendData send_data, int pos) {
    pollfd fds[2];
    bool waiting_for_unpause = false;
    while (true) {
        fds[0] = {.fd = to_pl[pos][0], .events = POLLIN, .revents = 0};
        fds[1] = {.fd = send_data.get_fd(), .events = static_cast<short>(waiting_for_unpause ? POLLRDHUP : POLLIN), .revents = 0};
        poll(fds, 2, -1);
        // on client_fd we'd get either disconnect or unwanted messages
        if (fds[1].revents & POLLRDHUP) {
            // waiting_for_unpause = true
            write(to_pl[pos][1], &PAUSE, 1);
            throw std::runtime_error("client disconnected");
        }
        if (fds[1].revents & POLLIN) {
            // waiting_for_unpause = false
            std::string msg;
            ssize_t read_len = get_line(send_data, msg);
            if (read_len > 0 && std::regex_match(msg, TRICK_REGEX)) {
                send_WRONG(send_data, game.get_trick_no());
                continue;
            }
            else if (read_len == 0)
                throw std::runtime_error("client disconnected");
            else
                throw std::runtime_error("got from client");
        }
        // else we got a message on pipe
        char msg = get_from_pipe(to_pl[pos][0]);
        switch (msg) {
            case PAUSE:
                waiting_for_unpause = true;
                break;
            case PLAY:
                waiting_for_unpause = false;
                break;
            default:
                return msg;
        }
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
        char msg = get_from_pipe(to_pl[pos][0]); // acknowledge game is paused
        while (msg != PAUSE) {
            // this is not busy waiting because PAUSE is guaranteed to be in the queue
            write(to_pl[pos][1], &msg, 1);
            msg = get_from_pipe(to_pl[pos][0]);
        }
        std::vector<Card> hand;
        while (!active.is_over()) {
            wait_for_deal(send_data, pos, hand);
            int trick_no = game.get_trick_no();
            while (trick_no <= 13) {
                char job = wait_for_turn(send_data, pos);
                if (job == TURN) {
                    const auto &trick = game.get_trick(trick_no);
                    send_TRICK(send_data, trick_no, trick);
                    while (true) {
                        try {
                            auto [no, v] = get_TRICK(send_data, pos, timeout * 1000);
                            if (v.size() != 1) {
                                write(to_pl[pos][1], &TURN, 1);
                                throw std::runtime_error("Incorrect answer to TRICK (cards no. >1)");
                            }
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
                        write(to_pl[(pos + 1) % 4][1], &TURN, 1);
                    wait_for_turn(send_data, pos); // waiting for end of trick, should get TAKE
                }
                else { // job == TAKE
                    // previous client on this seat played a card in this trick -> find it
                    const auto &trick = game.get_trick(trick_no);
                    for (const Card &c: trick)
                        if (std::find(hand.begin(), hand.end(), c) != hand.end())
                            std::erase(hand, c);
                }
                send_TAKEN(send_data, trick_no);
                trick_no++;
            }
            send_SCORE(send_data);
            increment_event_fd(pl_to_gm); // barrier
            get_from_pipe(to_pl[pos][0]); // gets PAUSE
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
    close(to_pl[pos][0]);
}

void game_master(std::ifstream &desc, const int &game_over_fd) {
    for (const auto fds: to_pl) {
        pipe(fds);
        write(fds[1], &PAUSE, 1);
    }
    int current_deal;
    char starting_client;
    while (get_deal(desc, current_deal, starting_client)) {
        active.wait_for_four();
        clear_event_fd(pl_to_gm);
        for (const auto fds: to_pl)
            write(fds[1], &PLAY, 1);
        int tricks = 0;
        while (tricks < 13) {
            write(to_pl[game.get_pos()][1], &TURN, 1);
            while (true) {
                decrement_event_fd(pl_to_gm); // trick or disconnect
                if (active.is_four()) {
                    tricks++;
                    for (const auto fds: to_pl)
                        write(fds[1], &TAKE, 1); // let players know trick is over
                    break;
                }
                else {
                    for (const auto fds: to_pl)
                        write(fds[1], &PAUSE, 1); // game paused
                    active.wait_for_four();
                    clear_event_fd(pl_to_gm);
                    for (const auto fds: to_pl)
                        write(fds[1], &PLAY, 1); // let players know game is back on
                }
            }
        }
        decrement_event_fd(pl_to_gm, 4); // de facto barrier
        if (desc.peek() == EOF)
            active.end_game();
        for (const auto fds: to_pl)
            write(fds[1], &PAUSE, 1);
    }
    // end the game
    increment_event_fd(game_over_fd);
    for (const auto &fds: to_pl)
        close(fds[1]);
    decrement_event_fd(pl_to_gm, 4); // de facto join (catches disconnects)
    close(pl_to_gm);
}