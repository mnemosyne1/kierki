#ifndef SERVER_PARSER
#define SERVER_PARSER

#include <unistd.h>
#include <iostream>
#include "err.h"

struct server_config {
    int port = 0;
    std::string filename;
    int timeout = 5;
};

namespace details {
    [[noreturn]] void end() {
        fatal("possible options:\n"
            "\t\t-p <value> port (optional, default: chosen automatically)\n"
            "\t\t-f <value> file (required)\n"
            "\t\t-t <value> timeout (optional, default: 5)\n");
    }
}

// in case of multiple declarations last one is chosen
server_config get_server_config (int argc, char *argv[]) {
    server_config ans;
    int opt;
    bool file_set = 0;
    while ((opt = getopt(argc, argv, "p:f:t:")) != -1) {
        switch (opt) {
            case 'p':
                ans.port = std::atoi(optarg);
                break;
            case 'f':
                ans.filename = optarg;
                file_set = 1;
                break;
            case 't':
                ans.timeout = std::atoi(optarg);
                break;
            default:
                details::end();
        }
    }
    if (!file_set)
        details::end();
    return ans;
}

#endif