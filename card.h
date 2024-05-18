#ifndef GAME_H
#define GAME_H

#include <string>
#include <compare>

enum class Value : char {
    C2,
    C3,
    C4,
    C5,
    C6,
    C7,
    C8,
    C9,
    C10,
    J,
    Q,
    K,
    A
};

class Card {
private:
    Value value;
    char suit;
public:
    Card(const int &value, const char &suit);
    explicit Card(std::string desc);
    // card of different suit is always considered worse
    std::strong_ordering operator<=>(const Card &other) const;
    std::string to_string();
};

#endif