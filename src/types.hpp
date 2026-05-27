#pragma once

struct Market{
    uint32_t id;
    uint32_t yes_bid;
    uint32_t yes_ask;
    uint32_t no_bid;
    uint32_t no_ask;
    uint32_t fee_rate;
    uint32_t available_size;
};

struct Constraint{
    uint32_t strong_market_id;
    uint32_t weak_market_id;
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
