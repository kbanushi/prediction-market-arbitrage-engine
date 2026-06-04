#pragma once

#include <cstdint>

enum class EventType : uint8_t {
    Add,
    Cancel,
    Modify,
    Trade,
    SnapshotLevel,
    Replace
};

enum class Side : char {
    Bid = 'B',
    Ask = 'A'
};

struct MarketEvent{
    uint64_t sequence_number = 0;
    uint64_t exchange_timestamp_ns = 0;

    uint64_t order_id = 0;
    uint32_t market_id = 0;
    
    EventType type = EventType::Add;
    Side side = Side::Bid;

    uint32_t price = 0;
    uint32_t quantity = 0;
};