#include <algorithm>
#include <csignal>
#include <fstream>
#include <poll.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <sys/eventfd.h>

#include "parser.h"
#include "server_communication.h"
#include "server_players.h"

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);
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

    Log log;
    std::thread gm(game_master, std::ref(desc), game_over_fd);
    sockaddr_storage client_address{}, server_address{};
    socklen_t addr_size = (socklen_t){sizeof client_address};
    std::vector<std::thread> clients;

    do {
        poll (fds, 2, -1);
        if (fds[1].revents & POLLIN) { // the game is over, finish everything
            close(socket_fd);
            close(game_over_fd);
            break;
        }
        if (fds[0].revents & POLLIN) { // new client tries to connect
            fds[0].revents = 0;
            addr_size = (socklen_t){sizeof client_address};
            int client_fd = accept(socket_fd, (sockaddr *) &client_address, &addr_size);
            addr_size = (socklen_t){sizeof server_address};
            if (getsockname(client_fd, (sockaddr *) &server_address, &addr_size)) {
                error("getsockname");
                close(client_fd);
                continue;
            }
            clients.emplace_back(handle_player, client_fd, client_address,
                                server_address, config.timeout, std::ref(log));
        }
    } while (true);
    gm.join();
    for (auto &client: clients)
        client.join();
    desc.close();
    std::stable_sort(log.begin(), log.end(), [](const Log_message &v1, const Log_message &v2){
        return v1.first.tv_sec != v2.first.tv_sec ? (v1.first.tv_sec < v2.first.tv_sec) : (v1.first.tv_nsec < v2.first.tv_nsec);
    });
    for (const auto& l: log)
        std::cout << l.second;

    return 0;
}