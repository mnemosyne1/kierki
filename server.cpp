#include <array>
#include <fstream>
#include <iostream>
#include <poll.h>
#include <thread>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/fcntl.h>

#include "parser.h"
#include "server_communication.h"
#include "server_gm.h"
#include "server_players.h"

void create_pipes(std::array<int, 4> &sends, std::array<int, 4> &rcvs) {
    int fds[2];
    for (int i = 0; i < 4; i++) {
        if (pipe2(fds, O_DIRECT) < 0)
            syserr("couldn't create a pipe");
        rcvs[i] = fds[0];
        sends[i] = fds[1];
    }
}

int main(int argc, char *argv[]) {
    server_config config = get_server_config(argc, argv);
    std::ifstream desc(config.filename);
    if (!desc)
        syserr("description not found");
    int socket_fd = socket_init(config.port);

    int game_over_fd = eventfd(0, 0);
    if (game_over_fd == -1)
        syserr("couldn't create eventfd");
    pollfd fds[2];
    fds[0] = {.fd = socket_fd, .events = POLLIN, .revents = 0};
    fds[1] = {.fd = game_over_fd, .events = POLLIN, .revents = 0};

    std::array<int, 4> gm_rcv{}, gm_send{}, from_gm{}, to_gm{};
    create_pipes(to_gm, gm_rcv);
    create_pipes(gm_send, from_gm);
    std::thread gm(game_master, gm_rcv, gm_send, std::ref(desc), game_over_fd);
    sockaddr_in6 client_address{};
    socklen_t addr_size = (socklen_t){sizeof client_address};

    do {
        poll (fds, 2, -1);
        if (fds[1].revents & POLLIN) { // the game is over, finish everything
            close(socket_fd);
            close(game_over_fd);
            break;
        }
        if (fds[0].revents & POLLIN) { // new client tries to connect
            fds[0].revents = 0;
            int client_fd = accept(socket_fd,
                                   (struct sockaddr *) &client_address,
                                   &addr_size);
            std::thread client(handle_player, client_fd, client_address, to_gm, from_gm, config.timeout);
            client.detach();
        }
    } while (true);
    gm.join();
    desc.close();

    return 0;
}