#ifndef GAME_H
#define GAME_H

#include <compare>
#include <cstdint>
#include <string>
#include <vector>

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
    // card of different suit is always considered worse
    bool operator<(const Card &other) const;
    bool operator==(const Card &other) const = default;
    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] Suit get_suit() const noexcept;
};

constexpr std::vector<Card> parse_cards(const std::string &input) {
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

constexpr std::string cards_to_string(const std::vector<Card> &cards) {
    std::string ans;
    for (const Card &c : cards)
        ans += c.to_string();
    return ans;
}

#endif