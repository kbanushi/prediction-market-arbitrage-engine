#include <catch2/catch_test_macros.hpp>

#include "../src/book_builder.hpp"
#include "../src/market_event.hpp"
#include "../src/order_book.hpp"
#include "../src/spsc_queue.hpp"

namespace {

MarketEvent make_event(
    uint64_t sequence_number,
    uint64_t order_id,
    EventType type,
    Side side,
    uint32_t price,
    uint32_t quantity
) {
    MarketEvent event{};
    event.sequence_number = sequence_number;
    event.exchange_timestamp_ns = 0;
    event.order_id = order_id;
    event.market_id = 1;
    event.price = price;
    event.quantity = quantity;
    event.type = type;
    event.side = side;
    return event;
}

} // namespace

TEST_CASE("BookBuilder applies ordered event to order book", "[book_builder][process_event]") {
    OrderBook book(1024);
    BookBuilder builder(book);

    MarketEvent event = make_event(
        1,
        1001,
        EventType::Add,
        Side::Bid,
        5000,
        10
    );

    BookBuildResult result = builder.process_event(event);

    REQUIRE(result.status == BookBuildStatus::Applied);
    REQUIRE(result.expected_sequence == 1);
    REQUIRE(result.actual_sequence == 1);

    REQUIRE(builder.last_sequence_number() == 1);
    REQUIRE(book.get_best_bid() == 5000);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 10);
}

TEST_CASE("BookBuilder detects duplicate event without mutating sequence state", "[book_builder][process_event]") {
    OrderBook book(1024);
    BookBuilder builder(book);

    MarketEvent event = make_event(
        1,
        1001,
        EventType::Add,
        Side::Bid,
        5000,
        10
    );

    BookBuildResult first = builder.process_event(event);
    BookBuildResult duplicate = builder.process_event(event);

    REQUIRE(first.status == BookBuildStatus::Applied);
    REQUIRE(duplicate.status == BookBuildStatus::Duplicate);

    REQUIRE(duplicate.expected_sequence == 2);
    REQUIRE(duplicate.actual_sequence == 1);

    REQUIRE(builder.last_sequence_number() == 1);
    REQUIRE(book.get_best_bid() == 5000);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 10);
}

TEST_CASE("BookBuilder detects sequence gap and does not apply event", "[book_builder][process_event]") {
    OrderBook book(1024);
    BookBuilder builder(book);

    MarketEvent event = make_event(
        2,
        1001,
        EventType::Add,
        Side::Bid,
        5000,
        10
    );

    BookBuildResult result = builder.process_event(event);

    REQUIRE(result.status == BookBuildStatus::SequenceGap);
    REQUIRE(result.expected_sequence == 1);
    REQUIRE(result.actual_sequence == 2);

    REQUIRE(builder.last_sequence_number() == 0);
    REQUIRE(book.get_best_bid() == -1);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 0);
}

TEST_CASE("BookBuilder does not advance sequence for unsupported event", "[book_builder][process_event]") {
    OrderBook book(1024);
    BookBuilder builder(book);

    MarketEvent event = make_event(
        1,
        1001,
        EventType::Trade,
        Side::Bid,
        5000,
        10
    );

    BookBuildResult result = builder.process_event(event);

    REQUIRE(result.status == BookBuildStatus::UnsupportedEvent);
    REQUIRE(result.expected_sequence == 1);
    REQUIRE(result.actual_sequence == 1);

    REQUIRE(builder.last_sequence_number() == 0);
    REQUIRE(book.get_best_bid() == -1);
    REQUIRE(book.get_best_ask() == -1);
}

TEST_CASE("BookBuilder applies ordered add modify cancel sequence", "[book_builder][process_event]") {
    OrderBook book(1024);
    BookBuilder builder(book);

    REQUIRE(builder.process_event(
        make_event(1, 1001, EventType::Add, Side::Bid, 5000, 10)
    ).status == BookBuildStatus::Applied);

    REQUIRE(builder.process_event(
        make_event(2, 1001, EventType::Modify, Side::Bid, 5050, 15)
    ).status == BookBuildStatus::Applied);

    REQUIRE(builder.process_event(
        make_event(3, 1001, EventType::Cancel, Side::Bid, 0, 0)
    ).status == BookBuildStatus::Applied);

    REQUIRE(builder.last_sequence_number() == 3);
    REQUIRE(book.get_best_bid() == -1);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 0);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5050) == 0);
}

TEST_CASE("BookBuilder drains ordered events from queue", "[book_builder][drain]") {
    OrderBook book(1024);
    BookBuilder builder(book);
    SPSCQueue<MarketEvent, 1024> queue;

    REQUIRE(queue.push(make_event(1, 1001, EventType::Add, Side::Bid, 5000, 10)));
    REQUIRE(queue.push(make_event(2, 1002, EventType::Add, Side::Ask, 5100, 20)));
    REQUIRE(queue.push(make_event(3, 1001, EventType::Modify, Side::Bid, 5050, 15)));

    BookDrainResult result = builder.drain(queue);

    REQUIRE(result.status == BookDrainStatus::Drained);
    REQUIRE(result.events_consumed == 3);
    REQUIRE(result.events_applied == 3);

    REQUIRE(builder.last_sequence_number() == 3);
    REQUIRE(book.get_best_bid() == 5050);
    REQUIRE(book.get_best_ask() == 5100);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5050) == 15);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 20);
}

TEST_CASE("BookBuilder drain ignores duplicate events and continues", "[book_builder][drain]") {
    OrderBook book(1024);
    BookBuilder builder(book);
    SPSCQueue<MarketEvent, 1024> queue;

    REQUIRE(queue.push(make_event(1, 1001, EventType::Add, Side::Bid, 5000, 10)));
    REQUIRE(queue.push(make_event(1, 1001, EventType::Add, Side::Bid, 5000, 10)));
    REQUIRE(queue.push(make_event(2, 1002, EventType::Add, Side::Ask, 5100, 20)));

    BookDrainResult result = builder.drain(queue);

    REQUIRE(result.status == BookDrainStatus::Drained);
    REQUIRE(result.events_consumed == 3);
    REQUIRE(result.events_applied == 2);

    REQUIRE(builder.last_sequence_number() == 2);
    REQUIRE(book.get_best_bid() == 5000);
    REQUIRE(book.get_best_ask() == 5100);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 10);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 20);
}

TEST_CASE("BookBuilder drain stops on sequence gap", "[book_builder][drain]") {
    OrderBook book(1024);
    BookBuilder builder(book);
    SPSCQueue<MarketEvent, 1024> queue;

    REQUIRE(queue.push(make_event(1, 1001, EventType::Add, Side::Bid, 5000, 10)));
    REQUIRE(queue.push(make_event(3, 1002, EventType::Add, Side::Ask, 5100, 20)));

    BookDrainResult result = builder.drain(queue);

    REQUIRE(result.status == BookDrainStatus::StoppedOnSequenceGap);
    REQUIRE(result.events_consumed == 2);
    REQUIRE(result.events_applied == 1);
    REQUIRE(result.expected_sequence == 2);
    REQUIRE(result.actual_sequence == 3);

    REQUIRE(builder.last_sequence_number() == 1);
    REQUIRE(book.get_best_bid() == 5000);
    REQUIRE(book.get_best_ask() == -1);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 10);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 0);
}

TEST_CASE("BookBuilder drain stops on unsupported event", "[book_builder][drain]") {
    OrderBook book(1024);
    BookBuilder builder(book);
    SPSCQueue<MarketEvent, 1024> queue;

    REQUIRE(queue.push(make_event(1, 1001, EventType::Add, Side::Bid, 5000, 10)));
    REQUIRE(queue.push(make_event(2, 1002, EventType::Trade, Side::Ask, 5100, 5)));

    BookDrainResult result = builder.drain(queue);

    REQUIRE(result.status == BookDrainStatus::StoppedOnUnsupportedEvent);
    REQUIRE(result.events_consumed == 2);
    REQUIRE(result.events_applied == 1);
    REQUIRE(result.expected_sequence == 2);
    REQUIRE(result.actual_sequence == 2);

    REQUIRE(builder.last_sequence_number() == 1);
    REQUIRE(book.get_best_bid() == 5000);
    REQUIRE(book.get_best_ask() == -1);
}

TEST_CASE("BookBuilder drain on empty queue returns drained with zero counts", "[book_builder][drain]") {
    OrderBook book(1024);
    BookBuilder builder(book);
    SPSCQueue<MarketEvent, 1024> queue;

    BookDrainResult result = builder.drain(queue);

    REQUIRE(result.status == BookDrainStatus::Drained);
    REQUIRE(result.events_consumed == 0);
    REQUIRE(result.events_applied == 0);
    REQUIRE(result.expected_sequence == 1);
    REQUIRE(result.actual_sequence == 0);

    REQUIRE(builder.last_sequence_number() == 0);
    REQUIRE(book.get_best_bid() == -1);
    REQUIRE(book.get_best_ask() == -1);
}

TEST_CASE("BookBuilder drain can be called repeatedly", "[book_builder][drain]") {
    OrderBook book(1024);
    BookBuilder builder(book);
    SPSCQueue<MarketEvent, 1024> queue;

    REQUIRE(queue.push(make_event(1, 1001, EventType::Add, Side::Bid, 5000, 10)));

    BookDrainResult first = builder.drain(queue);

    REQUIRE(first.status == BookDrainStatus::Drained);
    REQUIRE(first.events_consumed == 1);
    REQUIRE(first.events_applied == 1);
    REQUIRE(builder.last_sequence_number() == 1);

    REQUIRE(queue.push(make_event(2, 1002, EventType::Add, Side::Ask, 5100, 20)));
    REQUIRE(queue.push(make_event(3, 1001, EventType::Modify, Side::Bid, 5050, 15)));

    BookDrainResult second = builder.drain(queue);

    REQUIRE(second.status == BookDrainStatus::Drained);
    REQUIRE(second.events_consumed == 2);
    REQUIRE(second.events_applied == 2);
    REQUIRE(builder.last_sequence_number() == 3);

    REQUIRE(book.get_best_bid() == 5050);
    REQUIRE(book.get_best_ask() == 5100);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5050) == 15);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 20);
}