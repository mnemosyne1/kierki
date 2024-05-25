#include <sys/types.h>
#include <cmath>
#include <unistd.h>
#include <mutex>
#include <iomanip>
#include <iostream>
#include <arpa/inet.h>

#include "common.h"

constexpr timespec handle_time(struct msghdr* msg) {
    cmsghdr* cmsg = CMSG_FIRSTHDR(msg);
    while (cmsg != nullptr && cmsg->cmsg_level != SOL_SOCKET)
        cmsg = CMSG_NXTHDR(msg,cmsg);
    // assert (cmsg->cmsg_type == SO_TIMESTAMPNS || cmsg->cmsg_type == SO_TIMESTAMPING)
    auto *ts = (timespec *) CMSG_DATA(cmsg);
    return ts[0];
}

// Following two functions come from Stevens' "UNIX Network Programming" book.
// ...but have been adapted to this task by myself (JO)
// Read n bytes from a descriptor. Use in place of read() when fd is a stream socket.
ssize_t readn(SendData &send_data, void *vptr, size_t n) {
    ssize_t nleft, nread;
    char *ptr;

    ptr = (char*) vptr;
    nleft = n;
    timespec t{};
    while (nleft > 0) {
        msghdr tmp{};
        static char control[1024];
        iovec a = {.iov_base = ptr, .iov_len = static_cast<size_t>(nleft)};
        tmp.msg_iov = &a;
        tmp.msg_iovlen = 1;
        tmp.msg_control = control;
        tmp.msg_controllen = sizeof control;
        if ((nread = recvmsg(send_data.get_fd(), &tmp, 0)) < 0)
            return nread;     // When error, return < 0.
        else if (nread == 0)
            break;            // EOF
        t = handle_time(&tmp);

        nleft -= nread;
        ptr += nread;
    }
    send_data.log_message(static_cast<const char *>(vptr), t, false);
    return n - nleft;         // return >= 0
}

// Write n bytes to a descriptor.
ssize_t writen(SendData &send_data, const void *vptr, size_t n){
    ssize_t nleft, nwritten;
    const char *ptr;

    ptr = (const char*) vptr;  // Can't do pointer arithmetic on void*.
    nleft = n;
    timespec t{};
    while (nleft > 0) {
        if ((nwritten = write(send_data.get_fd(), ptr, nleft)) <= 0)
            return nwritten;  // error
        msghdr tmp{};
        static char control[1024];
        tmp.msg_iov = nullptr;
        tmp.msg_iovlen = 0;
        tmp.msg_control = control;
        tmp.msg_controllen = sizeof control;
        static ssize_t nread;
        if ((nread = recvmsg(send_data.get_fd(), &tmp, MSG_ERRQUEUE)) < 0)
            return nread;
        t = handle_time(&tmp);

        nleft -= nwritten;
        ptr += nwritten;
    }
    send_data.log_message(static_cast<const char *>(vptr), t, true);
    return n;
}

// returns <ip>:<port>, (ENDING WITH A COMMA)
std::string get_ip(const sockaddr_storage &address) {
    std::stringstream ss;
    auto* addr = (sockaddr_in6*)&address;
    if (IN6_IS_ADDR_V4MAPPED(&addr->sin6_addr)) { // IPv4
        auto* addr_4 = (sockaddr_in*) &address;
        ss << inet_ntoa(addr_4->sin_addr);
    }
    else { // IPv6
        char ip[INET6_ADDRSTRLEN];
        ss << inet_ntop(AF_INET6, &(addr->sin6_addr), ip, INET6_ADDRSTRLEN);
    }
    ss << ":" << ntohs(addr->sin6_port) << ",";
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
