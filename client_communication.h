#ifndef CLIENT_COMMUNICATION_H
#define CLIENT_COMMUNICATION_H
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include "common.h"

int socket_init (const std::string& host, const std::string &port,
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

bool send_IAM (SendData &send_data, const char &seat) {
    static constexpr ssize_t IAM_LEN = 6;
    const char msg[6] = {'I', 'A', 'M', seat, '\r', '\n'};
    return (writen(send_data, msg, IAM_LEN) == IAM_LEN);
}

std::string get_line (const int &socket_fd) {
    // FIXME: stringstream, \r\n?
    char c;
    std::string ans;
    do {
        read(socket_fd, &c, 1);
        ans += c;
    } while (c != '\n');
    return ans;
}

#endif //CLIENT_COMMUNICATION_H
