#ifndef SERVER_GAME_H
#define SERVER_GAME_H

#include <array>
#include <atomic>
#include <fstream>

void game_master(const std::array<int, 4> &from_pl_fds, const std::array<int, 4> &to_pl_fds,
                 std::ifstream &desc, const int &game_over_fd);

#endif //SERVER_GAME_H
