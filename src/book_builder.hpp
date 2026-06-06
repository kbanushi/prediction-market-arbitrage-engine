#pragma once

#include <cstdint>

#include "market_event.hpp"
#include "order_book.hpp"
#include "spsc_queue.hpp"

enum class BookBuildStatus : uint8_t{
    Applied,
    Duplicate,
    SequenceGap,
    UnsupportedEvent
};

enum class BookDrainStatus : uint8_t {
    Drained,
    StoppedOnSequenceGap,
    StoppedOnUnsupportedEvent
};

struct BookDrainResult{
    uint64_t expected_sequence;
    uint64_t actual_sequence;
    std::size_t events_consumed;
    std::size_t events_applied;
    BookDrainStatus status;
};

struct BookBuildResult{
    uint64_t expected_sequence;
    uint64_t actual_sequence;
    BookBuildStatus status;
};

class BookBuilder{
    public:
        explicit BookBuilder(OrderBook& book);
        BookBuildResult process_event(const MarketEvent& event);
        uint64_t last_sequence_number() const;

        template<size_t Capacity>
        BookDrainResult drain(SPSCQueue<MarketEvent, Capacity>& queue){
            std::size_t events_consumed = 0;
            std::size_t events_applied = 0;

            MarketEvent event{};

            while (queue.pop(event)){
                ++events_consumed;

                const BookBuildResult result = process_event(event);
                if (result.status == BookBuildStatus::Applied){
                    ++events_applied;
                    continue;
                }

                if (result.status == BookBuildStatus::Duplicate){
                    continue;
                }

                if (result.status == BookBuildStatus::SequenceGap) [[unlikely]] {
                    return BookDrainResult{
                        result.expected_sequence, 
                        result.actual_sequence, 
                        events_consumed, 
                        events_applied, 
                        BookDrainStatus::StoppedOnSequenceGap
                    };
                }

                if (result.status == BookBuildStatus::UnsupportedEvent) [[unlikely]] {
                    return BookDrainResult{
                        result.expected_sequence, 
                        result.actual_sequence, 
                        events_consumed, 
                        events_applied, 
                        BookDrainStatus::StoppedOnUnsupportedEvent
                    };
                }
            }

            return BookDrainResult{
                last_sequence_number_ + 1,
                last_sequence_number_,
                events_consumed,
                events_applied,
                BookDrainStatus::Drained
            };
        }
    
    private:
        OrderBook& book_;
        uint64_t last_sequence_number_ = 0;
};