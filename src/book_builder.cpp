#include "book_builder.hpp"

BookBuilder::BookBuilder(OrderBook& book) : book_(book), last_sequence_number_(0) {}

BookBuildResult BookBuilder::process_event(const MarketEvent& event){
    uint64_t expected_sequence = last_sequence_number_ + 1;

    if (event.sequence_number <= last_sequence_number_) [[unlikely]] {
        return BookBuildResult{expected_sequence, event.sequence_number, BookBuildStatus::Duplicate};
    }

    if (event.sequence_number != expected_sequence) [[unlikely]] {
        return BookBuildResult{expected_sequence, event.sequence_number, BookBuildStatus::SequenceGap};
    }

    const bool applied = book_.apply(event);
    
    if (!applied) [[unlikely]] {
        return BookBuildResult{expected_sequence, event.sequence_number, BookBuildStatus::UnsupportedEvent};
    }

    last_sequence_number_ = event.sequence_number;

    return BookBuildResult{expected_sequence, event.sequence_number, BookBuildStatus::Applied};
}

uint64_t BookBuilder::last_sequence_number() const {
    return last_sequence_number_;
}