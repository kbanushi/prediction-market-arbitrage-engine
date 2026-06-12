#pragma once

#include <cstdint>
#include "types.hpp"

enum class EventType : uint8_t {
    SnapshotBegin, //clear book state for this market before loading a fresh snapshot
    SnapshotLevel, // set one bid/ask price from a full snapshot
    SnapshotEnd, // marks that snapshot loading is complete
    LevelSet, // live WebSocket price_change 
    LevelClear, // clear a price level
    Trade, 
    TickSizeChange //Exchange says tick size changed; probably metadata handling later
};

struct MarketEvent{
    uint64_t sequence_number = 0;
    uint64_t exchange_timestamp_ns = 0;
    uint64_t receive_timestamp_ns = 0;

    uint32_t market_id = 0;

    uint32_t price = 0;
    uint32_t quantity = 0;
    
    EventType type = EventType::LevelSet;
    Side side = Side::Bid;
};