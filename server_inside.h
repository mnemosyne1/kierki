#ifndef SERVER_INSIDE_H
#define SERVER_INSIDE_H

#include <array>
#include <atomic>
#include <bitset>
#include <condition_variable>
#include <stdexcept>
#include <poll.h>
#include <unordered_map>
#include <unistd.h>

#include "card.h"
#include "common.h"

class ActiveMap {
private:
    std::bitset<4> active_map{0};
    std::atomic_flag game_over = ATOMIC_FLAG_INIT;
    std::mutex mutex_four;
    std::condition_variable cv_four;
public:
    [[nodiscard]] std::string setActive(char c) {
        if (game_over.test())
            return "NESW";
        std::string ans;
        std::unique_lock<std::mutex> lock(mutex_four);
        int pos = get_index_from_seat(c);
        if (active_map.test(pos)) {
            for (int i = 0; i < 4; i++)
                if (active_map.test(i))
                    ans += get_seat_from_index(i);
        }
        else {
            active_map.set(pos);
            cv_four.notify_one();
        }
        return ans;
    }

    void disconnect(const int &pos, const int &event_fd) {
        std::unique_lock<std::mutex> lock(mutex_four);
        active_map.reset(pos);
        increment_event_fd(event_fd);
    }

    void end_game() noexcept {
        game_over.test_and_set();
    }

    bool is_active(int pos) noexcept {
        std::unique_lock<std::mutex> lock(mutex_four);
        return active_map.test(pos);
    }

    bool is_four() noexcept {
        std::unique_lock<std::mutex> lock(mutex_four);
        return active_map.all();
    }

    void wait_for_four() {
        std::unique_lock<std::mutex> lock(mutex_four);
        if (!active_map.all())
            cv_four.wait(lock, [this]{return active_map.all();});
    }

    [[nodiscard]] bool is_over() const {
        return game_over.test();
    }
};

class GameState {
private:
    std::array<std::vector<Card>, 4> hands;
    int current_deal;
    char first_player;
    char player;
    std::array<std::vector<Card>, 13> tricks;
    std::array<char, 13> taken;
    std::unordered_map<char, int> points_deal;
    std::unordered_map<char, int> points_total = {{'N', 0}, {'E', 0}, {'S', 0}, {'W', 0}};
    int current_trick;
    static int get_card_points(int deal, Card c) {
        int ans = 0;
        if ((deal == 2 || deal == 7) && c.get_suit() == Suit::H)
            ans++;
        if ((deal == 3 || deal == 7) && c.to_string()[0] == 'Q')
            ans += 5;
        if ((deal == 4 || deal == 7) && (c.to_string()[0] == 'J' || c.to_string()[0] == 'K'))
            ans += 2;
        if ((deal == 5 || deal == 7) && c.to_string() == "KH")
            ans += 18;
        return ans;
    }
public:
    void start_game(const int &pos, const std::vector<Card> &hand, int deal, char first) {
        hands[pos] = hand;
        current_deal = deal;
        first_player = player = first;
        current_trick = 0;
        points_deal = {{'N', 0}, {'E', 0}, {'S', 0}, {'W', 0}};
        tricks = std::array<std::vector<Card>, 13>();
        taken = std::array<char, 13>();
    }

    [[nodiscard]] std::vector<Card> get_hand(const int &pos) const noexcept {
        return hands[pos];
    }

    [[nodiscard]] char get_first() const noexcept {
        return first_player;
    }

    [[nodiscard]] int get_pos() const noexcept {
        return get_index_from_seat(player);
    }

    [[nodiscard]] int get_deal() const noexcept {
        return current_deal;
    }

    [[nodiscard]] int get_trick_no() const noexcept {
        return current_trick + 1;
    }

    [[nodiscard]] const std::vector<Card> &get_trick(int trick) const noexcept {
        return tricks[trick - 1];
    }

    void play(const Card &c) {
        tricks[current_trick].push_back(c);
        if (tricks[current_trick].size() == 4) {
            Card highest = tricks[current_trick][0];
            int start_pos = get_pos();
            for (int i = 1; i < 4; i++) {
                if (highest < tricks[current_trick][i]) {
                    highest = tricks[current_trick][i];
                    player = get_seat_from_index((start_pos + i) % 4);
                }
            }
            taken[current_trick] = player;
            for (const auto &card: tricks[current_trick]) {
                points_deal[player] += get_card_points(current_deal, card);
            }
            if (current_deal == 1 || current_deal == 7)
                points_deal[player]++; // point for each trick
            if (current_deal >= 6 && (current_trick == 7 || current_trick == 13))
                points_deal[player] += 10; // points for 7th and 13th trick
            current_trick++;
            if (current_trick == 13) // end of deal
                for (const auto &[k, v]: points_deal)
                    points_total[k] += v;
        }
    }

    std::string get_TAKEN(int trick) {
        std::stringstream ss;
        ss << "TAKEN" << trick << cards_to_string(tricks[trick - 1]) << taken[trick - 1] << "\r\n";
        return ss.str();
    }

    std::string get_SCORE() {
        std::stringstream ss;
        ss << "SCORE";
        for (const auto &[k, v]: points_deal)
            ss << k << v;
        ss << "\r\n";
        return ss.str();
    }

    std::string get_TOTAL() {
        std::stringstream ss;
        ss << "TOTAL";
        for (const auto &[k, v]: points_total)
            ss << k << v;
        ss << "\r\n";
        return ss.str();
    }
};

#endif //SERVER_INSIDE_H
