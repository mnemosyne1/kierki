#ifndef SERVER_PLAYERS_H
#define SERVER_PLAYERS_H

#include <arpa/inet.h>
#include <array>

void handle_player(
    const int &client_fd,
    const sockaddr_in6 &client_address,
    const std::array<int, 4> &to_gm,
    const std::array<int, 4> &from_gm,
    const int &timeout
);

#endif //SERVER_PLAYERS_H
