#include <iostream>
#include <linux/net_tstamp.h>

#include "err.h"
#include "parser.h"
#include "client_communication.h"

int main(int argc, char *argv[]) {
    client_config config = get_client_config(argc, argv);
    sockaddr_storage server_address{}, client_address{};
    int socket_fd = socket_init(config.host, config.port, config.ipv, &server_address);
    int enable = SOF_TIMESTAMPING_TX_SOFTWARE |
                 SOF_TIMESTAMPING_RX_SOFTWARE |
                 SOF_TIMESTAMPING_SOFTWARE;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_TIMESTAMPING, &enable, sizeof(int)) < 0) {
        close(socket_fd);
        syserr("setsockopt");
    }
    auto addr_size = static_cast<socklen_t>(sizeof client_address);
    if (getsockname(socket_fd, (sockaddr *) &client_address, &addr_size)) {
        close(socket_fd);
        syserr("getsockname");
    }
    SendData send_data(socket_fd, client_address, server_address);
    if (!send_IAM(send_data, config.seat)) {
        close(socket_fd);
        syserr("couldn't send IAM");
    }
    std::string deal = get_line(socket_fd);
    std::cerr << deal;
    //sleep(10);
    //send_data.print_log();
    close(socket_fd);
}