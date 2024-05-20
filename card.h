#ifndef GAME_H
#define GAME_H

#include <cstdint>
#include <string>
#include <compare>

enum class Value : uint8_t {
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
enum class Suit : uint8_t {C, D, H, S};

class Card {
private:
    Value value;
    Suit suit;
public:
    Card(const int &value, const int &suit);
    explicit Card(std::string desc);
    //explicit Card(const uint8_t &hash);
    // card of different suit is always considered worse
    std::strong_ordering operator<=>(const Card &other) const;
    [[nodiscard]] std::string to_string() const;
    //[[nodiscard]] uint8_t hash() const;
};

#endif