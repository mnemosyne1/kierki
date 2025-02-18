#ifndef SERVER_PARSER
#define SERVER_PARSER

#include <sys/socket.h>
#include <unistd.h>
#include "err.h"

struct server_config {
    int port = 0;
    std::string filename;
    int timeout = 5;
};

struct client_config {
    std::string host;
    std::string port;
    int ipv = AF_UNSPEC;
    char seat = 0;
    bool auto_player = false;
};

namespace details {
    [[noreturn]] void usage_server() {
        fatal("possible options:\n"
            "\t\t-p <value> port (optional, default: chosen automatically)\n"
            "\t\t-f <value> file (required)\n"
            "\t\t-t <value> timeout (optional, default: 5)\n");
    }

    [[noreturn]] void usage_client() {
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
server_config get_server_config (int argc, char *argv[]) {
    server_config ans;
    int opt;
    bool file_set = false;
    while ((opt = getopt(argc, argv, "p:f:t:")) != -1) {
        switch (opt) {
            case 'p':
                ans.port = std::stoi(optarg);
                break;
            case 'f':
                ans.filename = std::string(optarg);
                file_set = true;
                break;
            case 't':
                ans.timeout = std::stoi(optarg);
                break;
            default:
                details::usage_server();
        }
    }
    if (!file_set)
        details::usage_server();
    return ans;
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
                details::usage_client();
        }
    }
    if (!host_set || !port_set || !seat_set)
        details::usage_client();
    return ans;
}

#endif