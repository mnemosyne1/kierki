#include <cmath>
#include <unistd.h>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <regex>
#include <poll.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include "common.h"

constexpr timespec handle_time(struct msghdr* msg) {
    cmsghdr* cmsg = CMSG_FIRSTHDR(msg);
    while (cmsg != nullptr && cmsg->cmsg_level != SOL_SOCKET)
        cmsg = CMSG_NXTHDR(msg,cmsg);
    // assert (cmsg->cmsg_type == SO_TIMESTAMPNS || cmsg->cmsg_type == SO_TIMESTAMPING)
    auto *ts = (timespec *) CMSG_DATA(cmsg);
    return ts[0];
}

ssize_t get_line (SendData &send_data, size_t max_length, std::string &ans) {
    char c{}, prev;
    ssize_t nread;
    msghdr tmp{};
    auto now = std::chrono::system_clock::now(); // backup option
    do {
        static char control[1024];
        iovec a = {.iov_base = &c, .iov_len = 1};
        tmp.msg_iov = &a;
        tmp.msg_iovlen = 1;
        tmp.msg_control = control;
        tmp.msg_controllen = sizeof control;
        prev = c;
        if ((nread = recvmsg(send_data.get_fd(), &tmp, 0)) <= 0)
            return nread; // error
        ans += c;
        if (max_length-- == 0) {
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
    } while (c != '\n' && prev != '\r');
    //timespec t = handle_time(&tmp);
    auto sec_point = std::chrono::time_point_cast<std::chrono::seconds>(now);
    auto sec = sec_point.time_since_epoch().count();
    auto nsec_point = std::chrono::time_point_cast<std::chrono::nanoseconds>(now)
                      - std::chrono::time_point_cast<std::chrono::nanoseconds>(sec_point);
    auto nsec = nsec_point.count();
    timespec t = {.tv_sec = sec, .tv_nsec = nsec};
    send_data.log_message(ans.c_str(), t, false);
    return 1;
}

// Following function comes from Stevens' "UNIX Network Programming" book...
// ...but has been adapted to this task by myself (JO)
// Write n bytes to a descriptor.
ssize_t writen(SendData &send_data, const void *vptr, size_t n){
    ssize_t nleft, nwritten;
    const char *ptr;

    ptr = (const char*) vptr;  // Can't do pointer arithmetic on void*.
    nleft = n;
    timespec t{};
    while (nleft > 0) {
        auto now = std::chrono::system_clock::now(); // backup option
        if ((nwritten = write(send_data.get_fd(), ptr, nleft)) <= 0)
            return nwritten;  // error
        /*msghdr tmp{};
        static char control[1024];
        static char msg[1024];
        iovec a = {.iov_base = msg, .iov_len = sizeof msg};
        tmp.msg_iov = &a;
        tmp.msg_iovlen = 1;
        tmp.msg_control = control;
        tmp.msg_controllen = sizeof control;
        static ssize_t nread;*/
        bool got_timestamp = false;
        // FIXME: usunąć całkiem albo przywrócić
        /*for (int i = 0; i < 10; i++) { // main check
            nread = recvmsg(send_data.get_fd(), &tmp, MSG_ERRQUEUE);
            if (nread <= 0) {
                if (nread < 0 && errno == EAGAIN)
                    continue;
                else
                    return nread;
            }
            else {
                //got_timestamp = true;
                t = handle_time(&tmp);
                break;
            }
        }*/
        if (!got_timestamp) {
            //std::cerr << "Opcja awaryjna\n";
            auto sec_point = std::chrono::time_point_cast<std::chrono::seconds>(now);
            auto sec = sec_point.time_since_epoch().count();
            auto nsec_point = std::chrono::time_point_cast<std::chrono::nanoseconds>(now)
                    - std::chrono::time_point_cast<std::chrono::nanoseconds>(sec_point);
            auto nsec = nsec_point.count();
            t = {.tv_sec = sec, .tv_nsec = nsec};
        }
        /*do {
            std::cerr << "Reading\n";
            nread = recvmsg(send_data.get_fd(), &tmp, MSG_ERRQUEUE);
            if (nread <= 0) {
                if (nread < 0 && errno == EAGAIN) {
                    t = {.tv_sec = 0, .tv_nsec = 0};
                    break;
                }
                //continue; TODO TMP
                else
                    return nread;
            }
        } while (nread <= 0);*/
        //t = handle_time(&tmp);

        nleft -= nwritten;
        ptr += nwritten;
    }
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

const std::string CARD_REGEX("((?:[1-9]|10|Q|J|K|A)(?:[CDHS]))");
constexpr std::string multiply_string(const std::string &input, int times) {
    std::string ans;
    while (times--)
        ans += input;
    return ans;
}

[[nodiscard]] std::string print_list(const std::vector<Card> &cards) {
    ssize_t len = static_cast<ssize_t>(cards.size()) - 1;
    std::stringstream ss;
    for (ssize_t i = 0; i < len; i++)
        ss << cards[i].to_string() << ", ";
    if (len >= 0)
        ss << cards[len].to_string();
    return ss.str();
}

char get_IAM(SendData &send_data) {
    static constexpr ssize_t IAM_SIZE = 6;
    static const std::regex IAM_REGEX("IAM[NESW]\r\n");
    std::string msg;
    if (get_line(send_data, IAM_SIZE, msg) < 0)
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

/*template <bool client>
std::pair<int, std::vector<Card>> get_TRICK(SendData &send_data, bool taken_allowed) {
    static const std::regex re("TRICK([1-9]|1[0-3])" +
        multiply_string(CARD_REGEX + '?', 4) + "\r\n");
    // TODO: DUPLICATE? (same general idea, different details)
    std::string trick;
    std::smatch matches;
    // TODO: magic const
    if (get_line(send_data, 100, trick) <= 0) {
        if (errno == EAGAIN)
            throw std::runtime_error(timeout_trick_msg);
        else
            throw std::runtime_error("couldn't receive TRICK");
    }
    if (std::regex_match(trick, matches, re)) {
        if constexpr (client)
            std::cout << "Trick: (" << matches[1].str() << ") ";
        std::vector<Card> cards;
        for (size_t i = 2; i < matches.size(); i++) {
            if (!matches[i].str().empty()) {
                cards.emplace_back(matches[i].str());
                if constexpr (client) {
                    std::cout << matches[i].str();
                    if (i < matches.size() - 1 && !matches[i + 1].str().empty())
                        std::cout << ", ";
                    //else
                    //    std::cout << std::endl;
                }
            }
        }
        return {std::stoi(matches[1].str()), cards};
    }
    else if (taken_allowed) {
        throw std::runtime_error("TAKEN (not implemented yet)"); // FIXME
    }
    else
        throw std::runtime_error("invalid TRICK: " + trick);
}

// explicit initialisation
template std::pair<int, std::vector<Card>> get_TRICK<true>(SendData &send_data, bool taken_allowed);
template std::pair<int, std::vector<Card>> get_TRICK<false>(SendData &send_data, bool taken_allowed);
 */

void increment_event_fd(int event_fd, uint64_t val) {
    if (event_fd == 7)
        std::cerr << event_fd << '\n';
    if (event_fd == -1)
        throw std::runtime_error("initialising eventfd");
    if (write(event_fd, &val, sizeof val) != sizeof val)
        throw std::runtime_error("incrementing eventfd");
}

void decrement_event_fd(int event_fd, uint64_t times) {
    if (event_fd == 7)
        std::cerr << -event_fd << '\n';
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
    pollfd pfd = {.fd = event_fd, .events = POLLIN, .revents = 0};
    while (true) {
        poll(&pfd, 1, 0);
        if (pfd.revents & POLLIN)
            decrement_event_fd(event_fd);
        else
            return;
    }
}
