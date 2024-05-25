#ifndef SERVER_INSIDE_H
#define SERVER_INSIDE_H

#include <array>
#include <atomic>
#include <bitset>
#include <condition_variable>
#include <stdexcept>
#include <unistd.h>
#include <sys/eventfd.h>

#include "card.h"
#include "common.h"

void increment_event_fd(int event_fd, uint64_t val = 1) {
    if (event_fd == -1)
        throw std::runtime_error("initialising eventfd");
    if (write(event_fd, &val, sizeof val) != sizeof val)
        throw std::runtime_error("incrementing eventfd");
}

void decrement_event_fd(int event_fd) {
    if (event_fd == -1)
        throw std::runtime_error("initialising eventfd");
    uint64_t u;
    if (read(event_fd, &u, sizeof u) != sizeof u)
        throw std::runtime_error("decrementing eventfd");
}

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

    inline void end_game() noexcept {
        game_over.test_and_set();
    }

    void wait_for_four() {
        std::unique_lock<std::mutex> lock(mutex_four);
        if (!active_map.all())
            cv_four.wait(lock, [this]{return active_map.all();});
    }
};

class GameState {
private:
    std::array<std::vector<Card>, 4> hands;
    int current_deal{};
    std::atomic_char player;
public:
    void start_game(const int &pos, const std::vector<Card> &hand, int deal, char first) {
        hands[pos] = hand;
        current_deal = deal;
        player.store(first);
    }

    [[nodiscard]] std::vector<Card> get_hand(const int &pos) const noexcept {
        return hands[pos];
    }

    [[nodiscard]] char get_next_player() const noexcept {
        return player.load();
    }

    [[nodiscard]] int get_deal() const noexcept {
        return current_deal;
    }
};

#endif //SERVER_INSIDE_H
