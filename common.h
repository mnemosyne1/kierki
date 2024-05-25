#ifndef MIM_COMMON_H
#define MIM_COMMON_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include "err.h"

// TODO: TMP
#include <mutex>

class SendData {
private:
    const int fd;
    std::vector<std::pair<timespec, std::string>> log;
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
    // TODO: TMP
    void print_log() {
        static std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        for (const auto& s: log)
            std::cerr << s.first.tv_nsec << ' ' << s.second;
    }
};

ssize_t readn(SendData &send_data, void *vptr, size_t n);

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

#endif
