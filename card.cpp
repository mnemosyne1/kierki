#include <stdexcept>
#include <array>
#include "card.h"

constexpr std::array<std::pair<std::string, Value>, 13> mapping = {{
    {"2", Value::C2},
    {"3", Value::C3},
    {"4", Value::C4},
    {"5", Value::C5},
    {"6", Value::C6},
    {"7", Value::C7},
    {"8", Value::C8},
    {"9", Value::C9},
    {"10", Value::C10},
    {"J", Value::J},
    {"Q", Value::Q},
    {"K", Value::K},
    {"A", Value::A}
}};

constexpr Value get_value (const std::string &s) {
    for (const auto &p : mapping)
        if (p.first == s)
            return p.second;
    throw std::invalid_argument("Not a value");
}

#include <iostream>

constexpr std::string get_from_value (const Value &v) {
    for (const auto &p : mapping)
        if (p.second == v)
            return p.first;
    throw std::invalid_argument("Not a value");
}

Card::Card(const int &value, const char &suit) :
    value(static_cast<Value>(value)), suit(suit) {}

Card::Card(std::string desc) {
    suit = desc[desc.size() - 1];
    desc.pop_back();
    value = get_value(desc);
}

std::strong_ordering Card::operator<=>(const Card &other) const {
    if (suit != other.suit)
        return std::strong_ordering::greater;
    return value <=> other.value;
}

std::string Card::to_string() {
    return get_from_value(value) + suit;
}

