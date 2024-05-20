#include <stdexcept>
#include <array>
#include <utility>
#include "card.h"

constexpr std::array<std::pair<std::string, Value>, 13> mapping_val = {{
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
constexpr std::array<std::pair<char, Suit>, 13> mapping_suit = {{
    {'C', Suit::C},
    {'D', Suit::D},
    {'H', Suit::H},
    {'S', Suit::S}
}};

constexpr Value get_value (const std::string &s) {
    for (const auto &p : mapping_val)
        if (p.first == s)
            return p.second;
    throw std::invalid_argument("Not a value");
}

constexpr std::string get_from_value (const Value &v) {
    for (const auto &p : mapping_val)
        if (p.second == v)
            return p.first;
    throw std::invalid_argument("Not a value");
}

constexpr Suit get_suit (const char &s) {
    for (const auto &p : mapping_suit)
        if (p.first == s)
            return p.second;
    throw std::invalid_argument("Not a value");
}

constexpr char get_from_suit (const Suit &v) {
    for (const auto &p : mapping_suit)
        if (p.second == v)
            return p.first;
    throw std::invalid_argument("Not a value");
}

Card::Card(const int &value, const int &suit) :
    value(static_cast<Value>(value)), suit(static_cast<Suit>(suit)) {}

Card::Card(std::string desc) {
    suit = get_suit(desc[desc.size() - 1]);
    desc.pop_back();
    value = get_value(desc);
}

std::strong_ordering Card::operator<=>(const Card &other) const {
    if (suit != other.suit)
        return std::strong_ordering::greater;
    return value <=> other.value;
}

std::string Card::to_string() const {
    return get_from_value(value) + get_from_suit(suit);
}

/*// any in range [13, 81] should work - we need hash to be unique and in range of uint8_t
constexpr uint8_t hash_const = 20;

uint8_t Card::hash() const {
    return std::to_underlying(suit) * hash_const + std::to_underlying(value);
}

Card::Card(const uint8_t &hash) :
    value(static_cast<Value>(hash % hash_const)),
    suit(static_cast<Suit>(hash / hash_const)) {}*/
