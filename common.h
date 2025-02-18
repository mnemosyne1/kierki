#ifndef MIM_COMMON_H
#define MIM_COMMON_H

#include <cstddef>
#include <cstdint>
#include <regex>
#include <vector>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include "card.h"
#include "err.h"

// CLASSES

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

// SEAT-INDEX MAPPING

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

// COMMUNICATION

ssize_t writen(SendData &send_data, const void *vptr, size_t n);
ssize_t get_line(SendData &send_data, std::string &ans, size_t max_length = 100);
void increment_event_fd(int event_fd, uint64_t val = 1);
void decrement_event_fd(int event_fd, uint64_t times = 1);
void clear_event_fd(int event_fd);
const std::string CARD_REGEX("((?:[1-9]|10|Q|J|K|A)(?:[CDHS]))");
constexpr std::string multiply_string(const std::string &input, int times) {
    std::string ans;
    while (times--)
        ans += input;
    return ans;
}

// COMMUNICATION

void send_TRICK(SendData &send_data, int no, const std::vector<Card> &trick);
char get_IAM(SendData &send_data);
const std::string timeout_trick_msg = "timeout on receiving TRICK";

// REGEXES

const std::regex BUSY_REGEX("BUSY" + multiply_string("([NESW])?", 4) + "\r\n");
const std::regex DEAL_REGEX ("DEAL([1-7])([NESW])" + multiply_string(CARD_REGEX, 13) + "\r\n");
const std::regex TRICK_REGEX ("TRICK([1-9]|1[0-3])" +
                              multiply_string(CARD_REGEX + '?', 4) + "\r\n");
const std::regex WRONG_REGEX ("WRONG([1-9|1[0-3])\r\n");
const std::regex TAKEN_REGEX ("TAKEN([1-9]|1[0-3])" +
                              multiply_string(CARD_REGEX, 4) + "([NESW])\r\n");
const std::regex SCORE_REGEX("SCORE" + multiply_string("([NESW])(\\d+)", 4) + "\r\n");
const std::regex TOTAL_REGEX("TOTAL" + multiply_string("([NESW])(\\d+)", 4) + "\r\n");

// MESSAGE IDS

constexpr int BUSY = 0;
constexpr int DEAL = BUSY + 1; // 1
constexpr int TRICK = DEAL + 1; // 2
constexpr int WRONG = TRICK + 1; // 3
constexpr int TAKEN = WRONG + 1; // 4
constexpr int SCORE = TAKEN + 1; // 5
constexpr int TOTAL = SCORE + 1; // 6
constexpr int INCORRECT = TOTAL + 1; // 7
constexpr int REGEXES_NO = 7;
const std::regex regexes[REGEXES_NO] = {BUSY_REGEX, DEAL_REGEX,
                                        TRICK_REGEX, WRONG_REGEX,
                                        TAKEN_REGEX, SCORE_REGEX,
                                        TOTAL_REGEX};

#endif
