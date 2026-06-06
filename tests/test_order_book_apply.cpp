#include <catch2/catch_test_macros.hpp>

#include "../src/market_event.hpp"
#include "../src/order_book.hpp"

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

TEST_CASE("OrderBook apply inserts bid event", "[order_book][apply]") {
    OrderBook book(1024);

    MarketEvent event = make_event(
        1,
        1001,
        EventType::Add,
        Side::Bid,
        5000,
        10
    );

    REQUIRE(book.apply(event));

    REQUIRE(book.get_best_bid() == 5000);
    REQUIRE(book.get_best_ask() == -1);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 10);
}

TEST_CASE("OrderBook apply inserts ask event", "[order_book][apply]") {
    OrderBook book(1024);

    MarketEvent event = make_event(
        1,
        1001,
        EventType::Add,
        Side::Ask,
        5100,
        20
    );

    REQUIRE(book.apply(event));

    REQUIRE(book.get_best_bid() == -1);
    REQUIRE(book.get_best_ask() == 5100);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 20);
}

TEST_CASE("OrderBook apply cancels existing order", "[order_book][apply]") {
    OrderBook book(1024);

    REQUIRE(book.apply(make_event(1, 1001, EventType::Add, Side::Bid, 5000, 10)));
    REQUIRE(book.get_best_bid() == 5000);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 10);

    REQUIRE(book.apply(make_event(2, 1001, EventType::Cancel, Side::Bid, 0, 0)));

    REQUIRE(book.get_best_bid() == -1);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 0);
}

TEST_CASE("OrderBook apply modifies existing order price and quantity", "[order_book][apply]") {
    OrderBook book(1024);

    REQUIRE(book.apply(make_event(1, 1001, EventType::Add, Side::Bid, 5000, 10)));

    REQUIRE(book.apply(make_event(2, 1001, EventType::Modify, Side::Bid, 5050, 15)));

    REQUIRE(book.get_best_bid() == 5050);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 0);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5050) == 15);
}

TEST_CASE("OrderBook apply handles multiple bid levels", "[order_book][apply]") {
    OrderBook book(1024);

    REQUIRE(book.apply(make_event(1, 1001, EventType::Add, Side::Bid, 5000, 10)));
    REQUIRE(book.apply(make_event(2, 1002, EventType::Add, Side::Bid, 5050, 20)));
    REQUIRE(book.apply(make_event(3, 1003, EventType::Add, Side::Bid, 4950, 30)));

    REQUIRE(book.get_best_bid() == 5050);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 10);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5050) == 20);
    REQUIRE(book.get_volume_at_price(Side::Bid, 4950) == 30);
}

TEST_CASE("OrderBook apply handles multiple ask levels", "[order_book][apply]") {
    OrderBook book(1024);

    REQUIRE(book.apply(make_event(1, 1001, EventType::Add, Side::Ask, 5100, 10)));
    REQUIRE(book.apply(make_event(2, 1002, EventType::Add, Side::Ask, 5050, 20)));
    REQUIRE(book.apply(make_event(3, 1003, EventType::Add, Side::Ask, 5200, 30)));

    REQUIRE(book.get_best_ask() == 5050);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 10);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5050) == 20);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5200) == 30);
}

TEST_CASE("OrderBook apply returns false for unsupported event type", "[order_book][apply]") {
    OrderBook book(1024);

    MarketEvent trade_event = make_event(
        1,
        1001,
        EventType::Trade,
        Side::Bid,
        5000,
        10
    );

    REQUIRE_FALSE(book.apply(trade_event));

    REQUIRE(book.get_best_bid() == -1);
    REQUIRE(book.get_best_ask() == -1);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 0);
}

TEST_CASE("OrderBook apply leaves existing book unchanged after unsupported event", "[order_book][apply]") {
    OrderBook book(1024);

    REQUIRE(book.apply(make_event(1, 1001, EventType::Add, Side::Bid, 5000, 10)));

    MarketEvent unsupported = make_event(
        2,
        1002,
        EventType::SnapshotLevel,
        Side::Ask,
        5100,
        20
    );

    REQUIRE_FALSE(book.apply(unsupported));

    REQUIRE(book.get_best_bid() == 5000);
    REQUIRE(book.get_best_ask() == -1);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 10);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 0);
}