#pragma once

#include "types.hpp"
#include "order_book.hpp"
#include <array>
#include <stdexcept>
#include <algorithm>

constexpr uint32_t MAX_STRATEGY_MARKETS = 1024;
constexpr uint32_t MAX_CONSTRAINTS = MAX_STRATEGY_MARKETS / 2;

class Strategy {
private:
    std::array<Market, MAX_STRATEGY_MARKETS> market_pool{};
    std::array<Constraint, MAX_CONSTRAINTS> constraint_pool{};
    uint32_t constraint_count = 0;

public:
    Strategy() = default;

    void configure_market(uint32_t internal_index, uint32_t external_id, uint32_t fee_rate, OrderBook* book){
        if (internal_index >= MAX_STRATEGY_MARKETS)
            throw std::runtime_error("Fatal Configuration: Max strategies exceeded.");
        
        market_pool[internal_index] = Market{book, external_id, fee_rate,};
    }

    void configure_constraint(uint32_t strong_index, uint32_t weak_index){
        if (constraint_count >= MAX_CONSTRAINTS)
            throw std::runtime_error("Fatal Configuration: Max constraints exceeded.");

        constraint_pool[constraint_count++] = Constraint{strong_index, weak_index};
    }

    __attribute__((always_inline)) inline uint32_t evaluate_channels(Opportunity* const output_buffer, const uint32_t max_output_capacity) const {
        uint32_t opportunities_found = 0;

        for (uint32_t i = 0; i < constraint_count; i++){
            const Constraint& constraint = constraint_pool[i];

            const Market& strong_market = market_pool[constraint.strong_market_index];
            const Market& weak_market = market_pool[constraint.weak_market_index];

            int32_t strong_bid = strong_market.book->get_best_bid();
            int32_t weak_ask = weak_market.book->get_best_ask();

            if (strong_bid == -1 || weak_ask == -1) [[unlikely]] 
                continue;
            
            int32_t fees = (strong_bid * strong_market.fee_rate) / 10000 + (weak_ask * weak_market.fee_rate) / 10000;
            int32_t gross_edge = strong_bid - weak_ask;
            int32_t net_edge = gross_edge - fees;

            if (net_edge > 0){
                if (opportunities_found >= max_output_capacity) [[unlikely]]
                    break;

                Opportunity& opportunity = output_buffer[opportunities_found];
                opportunity.buy_market_id = weak_market.id;
                opportunity.sell_market_id = strong_market.id;
                opportunity.buy_price = weak_ask;
                opportunity.sell_price = strong_bid;
                opportunity.gross_edge = gross_edge;
                opportunity.net_edge = net_edge;
                opportunity.estimated_fees = fees;

                uint32_t strong_size = strong_market.book->get_volume_at_price('B', static_cast<uint32_t>(strong_bid));
                uint32_t weak_size = weak_market.book->get_volume_at_price('A', static_cast<uint32_t>(weak_ask));
                opportunity.max_size = std::min(strong_size, weak_size);

                opportunities_found++;
            }
        }

        return opportunities_found;
    }
};