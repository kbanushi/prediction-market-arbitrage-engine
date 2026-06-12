#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "../src/market_event.hpp"
#include "../src/order_book.hpp"

namespace {

MarketEvent make_event(
    uint64_t sequence_number,
    EventType type,
    Side side,
    uint32_t price,
    uint32_t quantity
) {
    MarketEvent event{};
    event.sequence_number = sequence_number;
    event.exchange_timestamp_ns = 0;
    event.receive_timestamp_ns = 0;
    event.market_id = 1;
    event.type = type;
    event.side = side;
    event.price = price;
    event.quantity = quantity;
    return event;
}

} // namespace

TEST_CASE("OrderBook apply sets bid level", "[order_book][apply]") {
    OrderBook book;

    MarketEvent event = make_event(
        1,
        EventType::LevelSet,
        Side::Bid,
        5000,
        10
    );

    REQUIRE(book.apply(event));

    REQUIRE(book.get_best_bid() == 5000);
    REQUIRE(book.get_best_ask() == -1);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 10);
}

TEST_CASE("OrderBook apply sets ask level", "[order_book][apply]") {
    OrderBook book;

    MarketEvent event = make_event(
        1,
        EventType::LevelSet,
        Side::Ask,
        5100,
        20
    );

    REQUIRE(book.apply(event));

    REQUIRE(book.get_best_bid() == -1);
    REQUIRE(book.get_best_ask() == 5100);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 20);
}

TEST_CASE("OrderBook apply updates existing level quantity", "[order_book][apply]") {
    OrderBook book;

    REQUIRE(book.apply(make_event(1, EventType::LevelSet, Side::Bid, 5000, 10)));
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 10);

    REQUIRE(book.apply(make_event(2, EventType::LevelSet, Side::Bid, 5000, 25)));

    REQUIRE(book.get_best_bid() == 5000);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 25);
}

TEST_CASE("OrderBook apply clears bid level", "[order_book][apply]") {
    OrderBook book;

    REQUIRE(book.apply(make_event(1, EventType::LevelSet, Side::Bid, 5000, 10)));
    REQUIRE(book.get_best_bid() == 5000);

    REQUIRE(book.apply(make_event(2, EventType::LevelClear, Side::Bid, 5000, 0)));

    REQUIRE(book.get_best_bid() == -1);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 0);
}

TEST_CASE("OrderBook apply clears ask level", "[order_book][apply]") {
    OrderBook book;

    REQUIRE(book.apply(make_event(1, EventType::LevelSet, Side::Ask, 5100, 20)));
    REQUIRE(book.get_best_ask() == 5100);

    REQUIRE(book.apply(make_event(2, EventType::LevelClear, Side::Ask, 5100, 0)));

    REQUIRE(book.get_best_ask() == -1);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 0);
}

TEST_CASE("OrderBook apply treats zero quantity LevelSet as clear", "[order_book][apply]") {
    OrderBook book;

    REQUIRE(book.apply(make_event(1, EventType::LevelSet, Side::Bid, 5000, 10)));
    REQUIRE(book.get_best_bid() == 5000);

    REQUIRE(book.apply(make_event(2, EventType::LevelSet, Side::Bid, 5000, 0)));

    REQUIRE(book.get_best_bid() == -1);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 0);
}

TEST_CASE("OrderBook apply handles multiple bid levels", "[order_book][apply]") {
    OrderBook book;

    REQUIRE(book.apply(make_event(1, EventType::LevelSet, Side::Bid, 5000, 10)));
    REQUIRE(book.apply(make_event(2, EventType::LevelSet, Side::Bid, 5050, 20)));
    REQUIRE(book.apply(make_event(3, EventType::LevelSet, Side::Bid, 4950, 30)));

    REQUIRE(book.get_best_bid() == 5050);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 10);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5050) == 20);
    REQUIRE(book.get_volume_at_price(Side::Bid, 4950) == 30);
}

TEST_CASE("OrderBook apply handles multiple ask levels", "[order_book][apply]") {
    OrderBook book;

    REQUIRE(book.apply(make_event(1, EventType::LevelSet, Side::Ask, 5100, 10)));
    REQUIRE(book.apply(make_event(2, EventType::LevelSet, Side::Ask, 5050, 20)));
    REQUIRE(book.apply(make_event(3, EventType::LevelSet, Side::Ask, 5200, 30)));

    REQUIRE(book.get_best_ask() == 5050);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 10);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5050) == 20);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5200) == 30);
}

TEST_CASE("OrderBook apply recomputes best bid after clearing current best", "[order_book][apply]") {
    OrderBook book;

    REQUIRE(book.apply(make_event(1, EventType::LevelSet, Side::Bid, 5000, 10)));
    REQUIRE(book.apply(make_event(2, EventType::LevelSet, Side::Bid, 5050, 20)));
    REQUIRE(book.get_best_bid() == 5050);

    REQUIRE(book.apply(make_event(3, EventType::LevelClear, Side::Bid, 5050, 0)));

    REQUIRE(book.get_best_bid() == 5000);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5050) == 0);
}

TEST_CASE("OrderBook apply recomputes best ask after clearing current best", "[order_book][apply]") {
    OrderBook book;

    REQUIRE(book.apply(make_event(1, EventType::LevelSet, Side::Ask, 5100, 10)));
    REQUIRE(book.apply(make_event(2, EventType::LevelSet, Side::Ask, 5050, 20)));
    REQUIRE(book.get_best_ask() == 5050);

    REQUIRE(book.apply(make_event(3, EventType::LevelClear, Side::Ask, 5050, 0)));

    REQUIRE(book.get_best_ask() == 5100);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5050) == 0);
}

TEST_CASE("OrderBook apply snapshot begin clears existing book", "[order_book][apply]") {
    OrderBook book;

    REQUIRE(book.apply(make_event(1, EventType::LevelSet, Side::Bid, 5000, 10)));
    REQUIRE(book.apply(make_event(2, EventType::LevelSet, Side::Ask, 5100, 20)));

    REQUIRE(book.apply(make_event(3, EventType::SnapshotBegin, Side::Bid, 0, 0)));

    REQUIRE(book.get_best_bid() == -1);
    REQUIRE(book.get_best_ask() == -1);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 0);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 0);
}

TEST_CASE("OrderBook apply snapshot levels rebuild book", "[order_book][apply]") {
    OrderBook book;

    REQUIRE(book.apply(make_event(1, EventType::SnapshotBegin, Side::Bid, 0, 0)));
    REQUIRE(book.apply(make_event(2, EventType::SnapshotLevel, Side::Bid, 5000, 10)));
    REQUIRE(book.apply(make_event(3, EventType::SnapshotLevel, Side::Ask, 5100, 20)));
    REQUIRE(book.apply(make_event(4, EventType::SnapshotEnd, Side::Bid, 0, 0)));

    REQUIRE(book.get_best_bid() == 5000);
    REQUIRE(book.get_best_ask() == 5100);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 10);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 20);
}

TEST_CASE("OrderBook apply returns false for unsupported trade event", "[order_book][apply]") {
    OrderBook book;

    MarketEvent trade_event = make_event(
        1,
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
    OrderBook book;

    REQUIRE(book.apply(make_event(1, EventType::LevelSet, Side::Bid, 5000, 10)));

    MarketEvent unsupported = make_event(
        2,
        EventType::TickSizeChange,
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