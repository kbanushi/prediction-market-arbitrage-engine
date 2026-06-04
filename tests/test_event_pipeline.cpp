#include <catch2/catch_test_macros.hpp>

#include "../src/order_book.hpp"
#include "../src/market_event.hpp"
#include "../src/spsc_queue.hpp"

TEST_CASE("Event pipeline applies normalized market events through SPSC queue", "[pipeline]") {
    SPSCQueue<MarketEvent, 1024> queue;
    OrderBook book(1024);

    REQUIRE(queue.push(MarketEvent{
        .sequence_number = 1,
        .market_id = 0,
        .order_id = 1001,
        .type = EventType::Add,
        .side = Side::Bid,
        .price = 5000,
        .quantity = 10
    }));

    REQUIRE(queue.push(MarketEvent{
        .sequence_number = 2,
        .market_id = 0,
        .order_id = 1002,
        .type = EventType::Add,
        .side = Side::Ask,
        .price = 5100,
        .quantity = 20
    }));

    REQUIRE(queue.push(MarketEvent{
        .sequence_number = 3,
        .market_id = 0,
        .order_id = 1001,
        .type = EventType::Modify,
        .side = Side::Bid,
        .price = 5050,
        .quantity = 15
    }));

    REQUIRE(queue.push(MarketEvent{
        .sequence_number = 4,
        .market_id = 0,
        .order_id = 1002,
        .type = EventType::Cancel,
        .side = Side::Ask,
        .price = 0,
        .quantity = 0
    }));

    MarketEvent event{};
    while (queue.pop(event)) {
        book.apply(event);
    }

    REQUIRE(book.get_best_bid() == 5050);
    REQUIRE(book.get_best_ask() == -1);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5050) == 15);
}