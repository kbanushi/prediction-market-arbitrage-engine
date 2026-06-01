#pragma once

class OrderBook;

struct Market{
    uint32_t id;
    uint32_t fee_rate;
    OrderBook* book;
    bool is_active = false;
};

struct Constraint{
    uint32_t strong_market_index;
    uint32_t weak_market_index;
};

struct Opportunity{
    uint32_t buy_market_id;
    uint32_t sell_market_id;
    uint32_t buy_price;
    uint32_t sell_price;
    int32_t gross_edge;
    int32_t net_edge;
    int32_t estimated_fees;
    uint32_t max_size;
};
