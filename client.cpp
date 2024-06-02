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
    try {
        send_IAM(send_data, config.seat);
        while (true) {
            std::string card;
            auto hand = get_DEAL(send_data);
            int score_total = 0;
            // TODO: poll with stdin
            while (score_total < 2) { // FIXME
                auto trick = get_TRICK<true>(send_data);
                std::cout << "\nAvailable: ";// << cards_to_string(hand) << std::endl;
                print_list(hand);
                // TODO: da się na starcie dostać TAKEN a nie TRICK
#if 0
                do {
                    std::cin >> card; // TODO: other options than !<card>
                    if (card[0] != '!' /*|| std::find(deal.begin(), deal.end(), Card(card.substr(1, card.size() - 1))) == deal.end()*/)
                        std::cout << "Invalid card\n";
                } while (card[0] != '!'); // FIXME
                send_TRICK(send_data, trick.first, std::vector<Card>{Card(card.substr(1, card.size()))});
#endif
                while (true) {
                    getline(std::cin, card);
                    try {
                        send_TRICK(send_data, trick.first, std::vector<Card>{Card(card)});
                        // send_data.print_log();
                        std::string ans;
                        if (get_line(send_data, 100, ans) < 0)
                            throw std::runtime_error("receiving TAKEN");
                        std::cout << ans; // TODO: TMP
                        if (ans.substr(0, 5) != "WRONG") { // TODO: TMP
                            std::erase(hand, Card(card));
                            break;
                        }
                    }
                    catch (const std::invalid_argument &) {
                        std::cout << "Invalid card\n";
                    }
                }
                if (trick.first == 13) {
                    while (score_total < 2) {
                        std::string ans;
                        if (get_line(send_data, 100, ans) < 0)
                            throw std::runtime_error("receiving SCORE");
                        std::cout << ans; // TODO: TMP
                        if (ans.starts_with("TOTAL") || ans.starts_with("SCORE"))
                            score_total++;
                    }
                }
            }
        }
    }
    catch (const std::runtime_error &e) {
        Log log;
        send_data.append_to_log(log);
        for (const Log_message &msg: log)
            std::cout << msg.second;
        if (std::string(e.what()) != "couldn't receive DEAL") {
            error(e.what());
            close(socket_fd);
            return 1;
        }
    }
    close(socket_fd);
    return 0;
}