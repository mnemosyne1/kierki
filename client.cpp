#include <iostream>

#include "err.h"
#include "parser.h"
#include "client_communication.h"

int main(int argc, char *argv[]) {
    client_config config = get_client_config(argc, argv);
    int socket_fd = socket_init(config.host, config.port, config.ipv);
    if (!send_IAM(socket_fd, config.seat))
        syserr("couldn't send IAM");
    std::string deal = get_line(socket_fd);
    std::cerr << deal;
    //sleep(10);
    close(socket_fd);
}