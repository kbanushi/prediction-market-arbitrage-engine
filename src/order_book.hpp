#pragma once

#include <array>
#include "market_event.hpp"


constexpr uint32_t MAX_PRICE_POINTS = 10000;

class OrderBook{
    private:
    std::array<uint32_t, MAX_PRICE_POINTS> bid_levels;
    std::array<uint32_t, MAX_PRICE_POINTS> ask_levels;

    int32_t best_bid_price = -1;
    int32_t best_ask_price = -1;

    void recompute_best_bid();
    void recompute_best_ask();

    public:
    OrderBook() = default;

    void set_level(Side side, uint32_t price, uint32_t quantity);
    void clear_level(Side side, uint32_t price);
    void clear();
    [[nodiscard]] bool apply(const MarketEvent& event);

    [[nodiscard]] int32_t get_best_bid() const;
    [[nodiscard]] int32_t get_best_ask() const;
    [[nodiscard]] uint32_t get_volume_at_price(Side side, uint32_t price) const;
};