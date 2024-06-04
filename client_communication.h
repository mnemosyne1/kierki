#ifndef CLIENT_COMMUNICATION_H
#define CLIENT_COMMUNICATION_H
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <regex>
#include "card.h"
#include "common.h"

int socket_init(const std::string& host, const std::string &port,
                const int &ipv, sockaddr_storage *server_address) {
    addrinfo hints, *server_info;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = ipv;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &server_info) != 0)
        syserr("cannot get server info");
    int socket_fd;
    socket_fd = socket(server_info->ai_family,
                       server_info->ai_socktype,
                       server_info->ai_protocol);
    if (socket_fd < 0)
        syserr("cannot create a socket");
    if (connect(socket_fd, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        close(socket_fd);
        syserr("cannot connect to server");
    }
    memcpy(server_address, server_info->ai_addr, server_info->ai_addrlen);
    freeaddrinfo(server_info);
    return socket_fd;
}

void send_IAM(SendData &send_data, const char &seat) {
    static constexpr ssize_t IAM_LEN = 6;
    const char msg[] = {'I', 'A', 'M', seat, '\r', '\n'};
    if (writen(send_data, msg, IAM_LEN) != IAM_LEN)
        throw std::runtime_error("sending IAM");
}

constexpr int BUSY = 0;
constexpr int DEAL = 1;
constexpr int TRICK = 2;
constexpr int WRONG = 3;
constexpr int TAKEN = 4;
constexpr int SCORE = 5;
constexpr int TOTAL = 6;
constexpr int INCORRECT = 7;
// FIXME: duplicate
const std::string CARD_REGEX("((?:[1-9]|10|Q|J|K|A)(?:[CDHS]))");
constexpr std::string multiply_string(const std::string &input, int times) {
    std::string ans;
    while (times--)
        ans += input;
    return ans;
}
const std::regex BUSY_REGEX("BUSY" + multiply_string("([NESW])?", 4) + "\r\n");
const std::regex DEAL_REGEX ("DEAL([1-7])([NESW])" + multiply_string(CARD_REGEX, 13) + "\r\n");
const std::regex TRICK_REGEX ("TRICK([1-9]|1[0-3])" +
                              multiply_string(CARD_REGEX + '?', 4) + "\r\n");
const std::regex WRONG_REGEX ("WRONG([1-9|1[0-3])\r\n");
const std::regex TAKEN_REGEX ("TAKEN([1-9]|1[0-3])" +
                              multiply_string(CARD_REGEX, 4) + "([NESW])\r\n");
const std::regex SCORE_REGEX("SCORE" + multiply_string("([NESW])(\\d+)", 4) + "\r\n");
const std::regex TOTAL_REGEX("TOTAL" + multiply_string("([NESW])(\\d+)", 4) + "\r\n");
constexpr int REGEXES_NO = 7;
const std::regex regexes[REGEXES_NO] = {BUSY_REGEX, DEAL_REGEX,
                                        TRICK_REGEX, WRONG_REGEX,
                                        TAKEN_REGEX, SCORE_REGEX,
                                        TOTAL_REGEX};

std::pair<int, std::string> get_message(SendData &send_data) {
    std::pair<int, std::string> ans{INCORRECT, ""};
    // TODO: magic const
    auto tmp = get_line(send_data, 100, ans.second);
    if (tmp <= 0) {
        std::cerr << tmp << ' ' << errno << '\n';
        throw std::runtime_error("couldn't receive message");
    }
    for (int i = 0; i < REGEXES_NO; i++) {
        if (std::regex_match(ans.second, regexes[i])) {
            ans.first = i;
            return ans;
        }
    }
    return ans;
}

#endif //CLIENT_COMMUNICATION_H
