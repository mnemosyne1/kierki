#ifndef MIM_COMMON_H
#define MIM_COMMON_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include "card.h"
#include "err.h"

typedef std::pair<timespec, std::string> Log_message;
typedef std::vector<Log_message> Log;
class SendData {
private:
    const int fd;
    Log log;
    const std::string sender_receiver;
    const std::string receiver_sender;
public:
    SendData(
            int fd,
            const sockaddr_storage &sender,
            const sockaddr_storage &receiver
    );
    [[nodiscard]] int get_fd() const noexcept;
    void log_message(const char *msg, const timespec &t, bool send);
    void append_to_log(Log &general_log);
};

ssize_t writen(SendData &send_data, const void *vptr, size_t n);

constexpr int get_index_from_seat(char seat) {
    switch (seat) {
        case 'N':
            return 0;
        case 'E':
            return 1;
        case 'S':
            return 2;
        case 'W':
            return 3;
        default:
            throw std::invalid_argument("not a valid seat");
    }
}

constexpr char get_seat_from_index(int index) {
    switch (index) {
        case 0:
            return 'N';
        case 1:
            return 'E';
        case 2:
            return 'S';
        case 3:
            return 'W';
        default:
            throw std::invalid_argument("not a valid index");
    }
}

ssize_t get_line(SendData &send_data, size_t max_length, std::string &ans);
[[nodiscard]] std::string print_list(const std::vector<Card> &cards);
void increment_event_fd(int event_fd, uint64_t val = 1);
void decrement_event_fd(int event_fd, uint64_t times = 1);
void clear_event_fd(int event_fd);

// COMMUNICATION
void send_TRICK(SendData &send_data, int no, const std::vector<Card> &trick);
char get_IAM(SendData &send_data);
//template <bool client>
//std::pair<int, std::vector<Card>> get_TRICK(SendData &send_data, bool taken_allowed=false);
const std::string timeout_trick_msg = "timeout on receiving TRICK";

#endif
