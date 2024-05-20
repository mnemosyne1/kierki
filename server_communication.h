#ifndef SERVER_COMMUNICATION_H
#define SERVER_COMMUNICATION_H

#include <arpa/inet.h>

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

#endif //SERVER_COMMUNICATION_H
