#ifndef SERVER_PARSER
#define SERVER_PARSER

#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include "err.h"

struct client_config {
    std::string host;
    std::string port;
    int ipv = AF_UNSPEC;
    char seat;
    bool auto_player = 0;
};

namespace details {
    [[noreturn]] void end() {
        fatal("possible options:\n"
            "\t\t-h <value> host (required)\n"
            "\t\t-p <value> port (required)\n"
            "\t\t-4 use IPv4 (optional)\n"
            "\t\t-6 use IPv6 (optional)\n"
            "\t\t-N/-E/-S/-W (required one of these)\n"
            "\t\t-a play automatically (optional)\n");
    }
}

// in case of multiple declarations last one is chosen
client_config get_client_config (int argc, char *argv[]) {
    client_config ans;
    int opt;
    bool host_set = false;
    bool port_set = false;
    bool seat_set = false;
    while ((opt = getopt(argc, argv, "h:p:46NESWa")) != -1) {
        switch (opt) {
            case 'h':
                ans.host = optarg;
                host_set = true;
                break;
            case 'p':
                ans.port = optarg;
                port_set = true;
                break;
            case '4':
                ans.ipv = AF_INET;
                break;
            case '6':
                ans.ipv = AF_INET6;
                break;
            case 'N':
            case 'E':
            case 'S':
            case 'W':
                ans.seat = static_cast<char>(opt);
                seat_set = true;
                break;
            case 'a':
                ans.auto_player = true;
                break;
            default:
                details::end();
        }
    }
    if (!host_set || !port_set || !seat_set)
        details::end();
    return ans;
}

#endif