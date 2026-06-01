#pragma once

#include <vector>
#include <array>
#include "ankerl_unordered_dense.hpp"

constexpr uint32_t MAX_PRICE_POINTS = 10000;

struct Order{
    uint64_t internal_id;
    uint32_t price;
    uint32_t quantity;

    int32_t prev_index = -1;
    int32_t next_index = -1;
};

struct PriceLevel{
    int32_t head_index = -1;
    int32_t tail_index = -1;
    uint32_t total_volume = 0;
};

class OrderBook{
    private:
    std::vector<Order> memory_pool;
    int32_t free_head;

    std::array<PriceLevel, MAX_PRICE_POINTS> bid_levels;
    std::array<PriceLevel, MAX_PRICE_POINTS> ask_levels;

    ankerl::unordered_dense::map<uint32_t, int32_t> id_to_index;

    int32_t best_bid_price = -1;
    int32_t best_ask_price = -1;

    int32_t allocate_order();
    void free_order(int32_t target_index);
    void remove_from_list(int32_t target_index, PriceLevel& level);

    public:
    explicit OrderBook(size_t max_active_orders = 100000);

    void insert_order(uint64_t internal_id, char size, uint32_t price, uint32_t quantity);
    void cancel_order(uint64_t internal_id, char size, uint32_t price);
    void modify_order(uint64_t internal_id, char side, uint32_t new_price, uint32_t quantity);

    [[nodiscard]] int32_t get_best_bid() const;
    [[nodiscard]] int32_t get_best_ask() const;
    [[nodiscard]] uint32_t get_volume_at_price(char side, uint32_t price) const;
};