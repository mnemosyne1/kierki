#ifndef SERVER_PLAYERS_H
#define SERVER_PLAYERS_H

#include <arpa/inet.h>
#include <array>
#include <fstream>
#include <vector>

typedef std::pair<timespec, std::string> Log_message;
typedef std::vector<Log_message> Log;

void handle_player(
    const int &client_fd,
    const sockaddr_storage &client_address,
    const sockaddr_storage &server_address,
    const int &timeout,
    Log &log
);

void game_master(std::ifstream &desc, const int &game_over_fd);

#endif //SERVER_PLAYERS_H
