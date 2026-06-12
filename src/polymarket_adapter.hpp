#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "market_event.hpp"
#include "spsc_queue.hpp"

namespace polymarket{
    constexpr uint32_t PRICE_SCALE = 10000;
    constexpr uint32_t SIZE_SCALE = 10000;

    enum class SnapshotParseStatus : uint8_t {
        Ok,
        InvalidJson,
        MissingField,
        InvalidPrice,
        InvalidSize,
        SnapshotTooLarge,
        QueueFull
    };

    struct SnapshotParseResult{
        uint64_t first_sequence, next_sequence;
        std::size_t events_emitted;
        SnapshotParseStatus status;
    };

    bool parse_scaled_decimal(std::string_view, uint32_t scale, uint32_t& out);

    template<std::size_t Capacity>
    SnapshotParseResult parse_book_snapshot(
        const std::string& payload, 
        uint32_t market_id, 
        uint64_t first_sequence, 
        SPSCQueue<MarketEvent, Capacity>& output_queue){

        using nlohmann::json;

        json parsed;

        try{
            parsed = json::parse(payload);
        } catch (...){
            return SnapshotParseResult{first_sequence, first_sequence, 0, SnapshotParseStatus::InvalidJson};
        }

        // validate json
        if (!parsed.contains("bids") || !parsed.contains("asks"))
            return SnapshotParseResult{first_sequence, first_sequence, 0, SnapshotParseStatus::MissingField};
        
        // validate json
        if (!parsed["bids"].is_array() || !parsed["asks"].is_array())
            return SnapshotParseResult{first_sequence, first_sequence, 0, SnapshotParseStatus::MissingField};
        

        const auto& bids = parsed["bids"];
        const auto& asks = parsed["asks"];

        const std::size_t required_events = 2 + bids.size() + asks.size();

        // Assumes queue is empty before ingestion 
        if (required_events > Capacity)
            return SnapshotParseResult{first_sequence, first_sequence, 0, SnapshotParseStatus::SnapshotTooLarge};
    
        uint64_t sequence = first_sequence;
        std::size_t events_emitted = 0;

        auto push_event = [&](EventType type, Side side, uint32_t price, uint32_t quantity) -> bool {
            MarketEvent event{};
            event.sequence_number = sequence++;
            event.exchange_timestamp_ns = 0;
            event.receive_timestamp_ns = 0;
            event.market_id = market_id;
            event.type = type;
            event.side = side;
            event.price = price;
            event.quantity = quantity;
            
            if (!output_queue.push(event))
                return false;

            ++events_emitted;
            return true;
        };

        // Queue full
        if (!push_event(EventType::SnapshotBegin, Side::Bid, 0, 0)) //Snapshot begin
            return SnapshotParseResult{first_sequence, sequence, events_emitted, SnapshotParseStatus::QueueFull};
        
        for (const auto& level : bids){
            if (!level.contains("price") || !level.contains("size"))
                return SnapshotParseResult{first_sequence, sequence, events_emitted, SnapshotParseStatus::MissingField};

            uint32_t price = 0, size = 0;

            if (!parse_scaled_decimal(level["price"].get<std::string>(), PRICE_SCALE, price))
                return SnapshotParseResult{first_sequence, sequence, events_emitted, SnapshotParseStatus::InvalidPrice};

            if (!parse_scaled_decimal(level["size"].get<std::string>(), SIZE_SCALE, size))
                return SnapshotParseResult{first_sequence, sequence, events_emitted, SnapshotParseStatus::InvalidSize};

            if (!push_event(EventType::SnapshotLevel, Side::Bid, price, size))
                return SnapshotParseResult{first_sequence, sequence, events_emitted, SnapshotParseStatus::QueueFull};
        }

        for (const auto& level : asks){
            if (!level.contains("price") || !level.contains("size"))
                return SnapshotParseResult{first_sequence, sequence, events_emitted, SnapshotParseStatus::MissingField};

            uint32_t price = 0, size = 0;

            if (!parse_scaled_decimal(level["price"].get<std::string>(), PRICE_SCALE, price))
                return SnapshotParseResult{first_sequence, sequence, events_emitted, SnapshotParseStatus::InvalidPrice};

            if (!parse_scaled_decimal(level["size"].get<std::string>(), SIZE_SCALE, size))
                return SnapshotParseResult{first_sequence, sequence, events_emitted, SnapshotParseStatus::InvalidSize};

            if (!push_event(EventType::SnapshotLevel, Side::Ask, price, size))
                return SnapshotParseResult{first_sequence, sequence, events_emitted, SnapshotParseStatus::QueueFull};
        }

        if (!push_event(EventType::SnapshotEnd, Side::Bid, 0, 0)) //Snapshot end
            return SnapshotParseResult{first_sequence, sequence, events_emitted, SnapshotParseStatus::QueueFull};

        return SnapshotParseResult{first_sequence, sequence, events_emitted, SnapshotParseStatus::Ok};
    }
} // namespace polymarket end