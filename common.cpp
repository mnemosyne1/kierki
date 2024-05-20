#include <sys/types.h>
#include <stddef.h>
#include <unistd.h>

#include "common.h"

// Following two functions come from Stevens' "UNIX Network Programming" book.
// Read n bytes from a descriptor. Use in place of read() when fd is a stream socket.
ssize_t readn(int fd, void *vptr, size_t n) {
    ssize_t nleft, nread;
    char *ptr;

    ptr = (char*) vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0)
            return nread;     // When error, return < 0.
        else if (nread == 0)
            break;            // EOF

        nleft -= nread;
        ptr += nread;
    }
    return n - nleft;         // return >= 0
}

// Write n bytes to a descriptor.
ssize_t writen(int fd, const void *vptr, size_t n){
    ssize_t nleft, nwritten;
    const char *ptr;

    ptr = (const char*) vptr;  // Can't do pointer arithmetic on void*.
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
            return nwritten;  // error

        nleft -= nwritten;
        ptr += nwritten;
    }
    return n;
}

int get_index_from_seat(char seat) {
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
            return -1;
    }
}

char get_seat_from_index(int index) {
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
            return 0;
    }
}

std::vector<Card> parse_cards(const std::string &input) {
    size_t i = 0;
    std::vector<Card> ans;
    while (i < input.size()) {
        if (input[i] == '1') { // must be 10
            ans.emplace_back(input.substr(i, 3)); // 10 + suit
            i += 3;
        } else {
            ans.emplace_back(input.substr(i, 2)); // (2-9/J/Q/K/A) + suit
            i += 2;
        }
    }
    return ans;
}

std::string cards_to_string(const std::vector<Card> &cards) {
    std::string ans;
    for (const auto &c : cards)
        ans += c.to_string();
    return ans;
}
