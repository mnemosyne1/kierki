#include <algorithm>
#include <csignal>
#include <fstream>
#include <iostream>
#include <poll.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <sys/eventfd.h>

#include "parser.h"
#include "server_threads.h"

constexpr int QUEUE_LENGTH = 10;

int socket_init (const int &port) {
    int socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (socket_fd < 0)
        syserr("cannot create a socket");
    sockaddr_in6 server_address {};
    server_address.sin6_family = AF_INET6;
    server_address.sin6_addr = in6addr_any; // Listening on all interfaces.
    server_address.sin6_port = htons(port);

    if (bind(socket_fd, (struct sockaddr *) &server_address, (socklen_t) sizeof server_address) < 0)
        syserr("bind");

    // Switch the socket to listening.
    if (listen(socket_fd, QUEUE_LENGTH) < 0)
        syserr("listen");

    // Find out what port the server is actually listening on.
    auto lenght = (socklen_t) sizeof server_address;
    if (getsockname(socket_fd, (struct sockaddr *) &server_address, &lenght) < 0)
        syserr("getsockname");
    std::cerr << "listening on port " << ntohs(server_address.sin6_port) << "\n";
    return socket_fd;
}

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
    for (std::thread &client: clients)
        client.join();
    desc.close();
    std::stable_sort(log.begin(), log.end(), [](const Log_message &v1, const Log_message &v2){
        return v1.first.tv_sec != v2.first.tv_sec ? (v1.first.tv_sec < v2.first.tv_sec) : (v1.first.tv_nsec < v2.first.tv_nsec);
    });
    for (const auto& l: log)
        std::cout << l.second;

    return 0;
}