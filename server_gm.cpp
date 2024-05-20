#include "server_gm.h"

#include <poll.h>
#include <unistd.h>
#include <vector>
#include "card.h"
#include "common.h"
#include "err.h"
#include "server_inside.h"

bool get_deal(std::ifstream &desc, int &current_deal,
              char &starting_client, std::array<std::vector<Card>, 4> &hands) {
    if (desc.eof()) {
        return false;
    }
    static std::string tmp;
    std::getline(desc, tmp);
    if (desc.eof())
        return false;
    current_deal = tmp[0] - '0';
    starting_client = tmp[1];
    for (auto &hand: hands) {
        std::getline(desc, tmp);
        hand = parse_cards(tmp);
    }
    return true;
}

void send_DEAL(const int &deal, const char &starting, const std::vector<Card> &hand, const int &fd) {
    std::string s = "DEAL" + std::to_string(deal) + starting + cards_to_string(hand) + "\r\n";
    if (write(fd, s.c_str(), s.size()) < 0)
        syserr("writing to pipe");
}

void game_master(const std::array<int, 4> &from_pl_fds, const std::array<int, 4> &to_pl_fds,
                 std::ifstream &desc, const int &game_over_fd) {
    //std::ifstream desc("gra.txt");
    int current_deal;
    char starting_client;
    std::array<std::vector<Card>, 4> hands;
    pollfd fds[4];
    for (int i = 0; i < 4; i++)
        fds[i] = {.fd = from_pl_fds[i], .events = POLLIN, .revents = 0};
    while (get_deal(desc, current_deal, starting_client, hands)) {
        int playercount = 0;
        do {
             poll(fds, 4, -1);
             for (auto & fd : fds) {
                 if (fd.revents & POLLIN) {
                     fd.revents = 0;
                     char tmp;
                     if (read(fd.fd, &tmp, 1) < 0)
                         syserr("readn");
                     if (tmp == CONNECT)
                         playercount++;
                     else if (tmp == DISCONNECT)
                         playercount--;
                     else
                         error("%c?", tmp);
                 }
             }
        } while (playercount < 4);
        for (int i = 0; i < 4; i++)
            send_DEAL(current_deal, starting_client, hands[i], to_pl_fds[i]);
    }
    // end the game
    uint64_t inc = 1;
    write(game_over_fd, &inc, sizeof inc);
}