#include <cmath>
#include <unistd.h>
#include <iomanip>
#include <mutex>
#include <poll.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include "common.h"

ssize_t get_line (SendData &send_data, std::string &ans, size_t max_length) {
    char c;
    ssize_t nread;
    auto now = std::chrono::system_clock::now();
    do {
        now = std::chrono::system_clock::now();
        if ((nread = read(send_data.get_fd(), &c, 1)) <= 0)
            return nread; // error
        if (c != 0)
            ans += c;
        if (max_length-- == 0 || c == 0) {
            auto sec_point = std::chrono::time_point_cast<std::chrono::seconds>(now);
            auto sec = sec_point.time_since_epoch().count();
            auto nsec_point = std::chrono::time_point_cast<std::chrono::nanoseconds>(now)
                              - std::chrono::time_point_cast<std::chrono::nanoseconds>(sec_point);
            auto nsec = nsec_point.count();
            timespec t = {.tv_sec = sec, .tv_nsec = nsec};
            ans += "\r\n";
            send_data.log_message(ans.c_str(), t, false);
            return -1;
        }
    } while (c != 0 && !std::isspace(c));
    if (c == '\r') {
        now = std::chrono::system_clock::now();
        if ((nread = read(send_data.get_fd(), &c, 1)) <= 0)
            return nread; // error
        if (c != 0)
            ans += c;
    }
    auto sec_point = std::chrono::time_point_cast<std::chrono::seconds>(now);
    auto sec = sec_point.time_since_epoch().count();
    auto nsec_point = std::chrono::time_point_cast<std::chrono::nanoseconds>(now)
                      - std::chrono::time_point_cast<std::chrono::nanoseconds>(sec_point);
    auto nsec = nsec_point.count();
    timespec t = {.tv_sec = sec, .tv_nsec = nsec};
    if (ans.ends_with("\r\n")) {
        send_data.log_message(ans.c_str(), t, false);
        return 1;
    }
    else {
        ans += "\r\n";
        send_data.log_message(ans.c_str(), t, false);
        return -1;
    }
}

// Following function comes from Stevens' "UNIX Network Programming" book...
// ...but has been adapted to this task by myself (JO)
// Write n bytes to a descriptor.
ssize_t writen(SendData &send_data, const void *vptr, size_t n) {
    ssize_t nleft, nwritten;
    const char *ptr;

    ptr = (const char*) vptr;  // Can't do pointer arithmetic on void*.
    nleft = n;
    auto now = std::chrono::system_clock::now();
    while (nleft > 0) {
        now = std::chrono::system_clock::now();
        if ((nwritten = write(send_data.get_fd(), ptr, nleft)) <= 0)
            return nwritten;  // error

        nleft -= nwritten;
        ptr += nwritten;
    }
    auto sec_point = std::chrono::time_point_cast<std::chrono::seconds>(now);
    auto sec = sec_point.time_since_epoch().count();
    auto nsec_point = std::chrono::time_point_cast<std::chrono::nanoseconds>(now)
                      - std::chrono::time_point_cast<std::chrono::nanoseconds>(sec_point);
    auto nsec = nsec_point.count();
    timespec t = {.tv_sec = sec, .tv_nsec = nsec};
    std::string msg(static_cast<const char *>(vptr), n);
    send_data.log_message(msg.c_str(), t, true);
    return n;
}

// returns <ip>:<port>, (ENDING WITH A COMMA)
std::string get_ip(const sockaddr_storage &address) {
    std::stringstream ss;
    if (address.ss_family == AF_INET) {
        auto *addr = (sockaddr_in *) &address;
        ss << inet_ntoa(addr->sin_addr) << ':' << ntohs(addr->sin_port) << ",";
    }
    else {
        auto *addr = (sockaddr_in6 *) &address;
        if (IN6_IS_ADDR_V4MAPPED(&addr->sin6_addr)) { // IPv4
            in_addr tmp = {.s_addr = addr->sin6_addr.s6_addr32[3]};
            ss << inet_ntoa(tmp);
        } else { // IPv6
            char ip[INET6_ADDRSTRLEN];
            ss << inet_ntop(AF_INET6, &(addr->sin6_addr), ip, INET6_ADDRSTRLEN);
        }
        ss << ":" << ntohs(addr->sin6_port) << ",";
    }
    return ss.str();
}

SendData::SendData(
        int fd,
        const sockaddr_storage &sender,
        const sockaddr_storage &receiver
) : fd(fd),
    sender_receiver(get_ip(sender) + get_ip(receiver)),
    receiver_sender(get_ip(receiver) + get_ip(sender)) {
}

int SendData::get_fd() const noexcept {
    return fd;
}

void SendData::log_message(const char *msg, const timespec &t, bool send) {
    std::stringstream ss;
    ss << '[' << (send ? sender_receiver : receiver_sender) <<
        std::put_time(std::localtime(&t.tv_sec), "%Y-%m-%dT%H:%M:%S") << '.' <<
        std::setw(3) << std::setfill('0') << t.tv_nsec / 1'000'000 << "] " << msg;
    log.emplace_back(t, ss.str());
}

static std::mutex mutex;
void SendData::append_to_log(Log &general_log) {
    std::unique_lock<std::mutex> lock(mutex);
    general_log.insert(general_log.end(), log.begin(), log.end());
}

char get_IAM(SendData &send_data) {
    static constexpr ssize_t IAM_SIZE = 6;
    static const std::regex IAM_REGEX("IAM[NESW]\r\n");
    std::string msg;
    if (get_line(send_data, msg, IAM_SIZE) < 0)
        throw std::runtime_error("receiving IAM");
    if (!std::regex_match(msg, IAM_REGEX))
        throw std::runtime_error("Invalid IAM message");
    return msg[3];
}

void send_TRICK(SendData &send_data, int no, const std::vector<Card> &trick) {
    std::string s = "TRICK" + std::to_string(no) + cards_to_string(trick) + "\r\n";
    if (writen(send_data, s.c_str(), s.size()) < static_cast<ssize_t>(s.size()))
        throw std::runtime_error("sending TRICK");
}

void increment_event_fd(int event_fd, uint64_t val) {
    if (event_fd == -1)
        throw std::runtime_error("initialising eventfd");
    if (write(event_fd, &val, sizeof val) != sizeof val)
        throw std::runtime_error("incrementing eventfd");
}

void decrement_event_fd(int event_fd, uint64_t times) {
    if (event_fd == -1)
        throw std::runtime_error("initialising eventfd");
    uint64_t u;
    while (times > 0) {
        if (read(event_fd, &u, sizeof u) != sizeof u)
            throw std::runtime_error("decrementing eventfd");
        times -= u;
    }
}

void clear_event_fd(int event_fd) {
    if (event_fd == -1)
        throw std::runtime_error("initialising eventfd");
    pollfd pfd{};
    while (true) {
        pfd = {.fd = event_fd, .events = POLLIN, .revents = 0};
        poll(&pfd, 1, 0);
        if (pfd.revents & POLLIN)
            decrement_event_fd(event_fd);
        else
            return;
    }
}
