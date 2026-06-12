#include <catch2/catch_test_macros.hpp>

#include "../src/order_book.hpp"
#include "../src/market_event.hpp"
#include "../src/spsc_queue.hpp"

TEST_CASE("Event pipeline applies normalized level events through SPSC queue", "[pipeline]") {
    SPSCQueue<MarketEvent, 1024> queue;
    OrderBook book;

    REQUIRE(queue.push(MarketEvent{
        .sequence_number = 1,
        .exchange_timestamp_ns = 0,
        .receive_timestamp_ns = 0,
        .market_id = 0,
        .type = EventType::LevelSet,
        .side = Side::Bid,
        .price = 5000,
        .quantity = 10
    }));

    REQUIRE(queue.push(MarketEvent{
        .sequence_number = 2,
        .exchange_timestamp_ns = 0,
        .receive_timestamp_ns = 0,
        .market_id = 0,
        .type = EventType::LevelSet,
        .side = Side::Ask,
        .price = 5100,
        .quantity = 20
    }));

    REQUIRE(queue.push(MarketEvent{
        .sequence_number = 3,
        .exchange_timestamp_ns = 0,
        .receive_timestamp_ns = 0,
        .market_id = 0,
        .type = EventType::LevelSet,
        .side = Side::Bid,
        .price = 5050,
        .quantity = 15
    }));

    REQUIRE(queue.push(MarketEvent{
        .sequence_number = 4,
        .exchange_timestamp_ns = 0,
        .receive_timestamp_ns = 0,
        .market_id = 0,
        .type = EventType::LevelClear,
        .side = Side::Ask,
        .price = 5100,
        .quantity = 0
    }));

    MarketEvent event{};
    while (queue.pop(event)) {
        REQUIRE(book.apply(event));
    }

    REQUIRE(book.get_best_bid() == 5050);
    REQUIRE(book.get_best_ask() == -1);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5050) == 15);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 10);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 0);
}