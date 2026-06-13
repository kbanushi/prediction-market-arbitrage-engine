#pragma once

#include <cstdint>
#include "types.hpp"

enum class TradeSide : uint8_t {
    Buy,
    Sell
};

enum class OrderStatus : uint8_t{
    New,
    Accepted,
    PartiallyFilled,
    Filled,
    Cancelled,
    Rejected
};

enum class TimeInForce : uint8_t{
    IOC,
    GTC
};

struct OrderIntent{
    uint64_t intent_id = 0;
    uint64_t created_timestamp_ns = 0;

    uint32_t internal_market_id = 0;
    uint32_t price = 0;
    uint32_t quantity = 0;

    TradeSide side = TradeSide::Buy;
    TimeInForce time_in_force = TimeInForce::IOC;
};

struct PaperOrder{
    uint64_t order_id = 0;
    uint64_t intent_id = 0;
    uint64_t created_timestamp_ns = 0;
    uint64_t updated_timestamp_ns = 0;

    uint32_t internal_market_id = 0;
    uint32_t price = 0;
    uint32_t original_quantity = 0;
    uint32_t remaining_quantity = 0;
    uint32_t filled_quantity = 0;

    TradeSide side = TradeSide::Buy;
    TimeInForce time_in_force = TimeInForce::IOC;
    OrderStatus status = OrderStatus::New;
};

enum class SubmitStatus : uint8_t{
    Accepted,
    CapacityExceeded,
    InvalidQuantity
};

struct SubmitResult{
    uint64_t order_id;
    SubmitStatus status = SubmitStatus::Accepted;
};

enum class CancelStatus : uint8_t{
    Cancelled,
    NotFound,
    AlreadyTerminal
};

enum class FillStatus : uint8_t{
    Filled,
    PartiallyFilled,
    NotFound,
    AlreadyTerminal,
    FillQuantityTooLarge,
    InvalidFillQuantity
};