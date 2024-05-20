#ifndef MIM_COMMON_H
#define MIM_COMMON_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include "err.h"
#include "card.h"

ssize_t readn(int fd, void *vptr, size_t n);

ssize_t writen(int fd, const void *vptr, size_t n);

int get_index_from_seat(char seat);

char get_seat_from_index(int index);

std::vector<Card> parse_cards(const std::string &input);

std::string cards_to_string(const std::vector<Card> &cards);

#endif
