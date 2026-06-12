#include <catch2/catch_test_macros.hpp>

#include "../src/book_builder.hpp"
#include "../src/market_event.hpp"
#include "../src/order_book.hpp"
#include "../src/polymarket_adapter.hpp"
#include "../src/spsc_queue.hpp"

TEST_CASE("Polymarket adapter parses scaled decimal prices", "[polymarket_adapter]") {
    uint32_t out = 0;

    REQUIRE(polymarket::parse_scaled_decimal("0.45", polymarket::PRICE_SCALE, out));
    REQUIRE(out == 4500);

    REQUIRE(polymarket::parse_scaled_decimal(".48", polymarket::PRICE_SCALE, out));
    REQUIRE(out == 4800);

    REQUIRE(polymarket::parse_scaled_decimal("1", polymarket::PRICE_SCALE, out));
    REQUIRE(out == 10000);

    REQUIRE(polymarket::parse_scaled_decimal("0.730", polymarket::PRICE_SCALE, out));
    REQUIRE(out == 7300);
}

TEST_CASE("Polymarket adapter converts book snapshot into market events", "[polymarket_adapter]") {
    const std::string payload = R"json(
    {
        "market": "0x123",
        "asset_id": "456",
        "timestamp": "1234567890",
        "hash": "abc",
        "bids": [
            { "price": "0.45", "size": "100" },
            { "price": "0.44", "size": "200" }
        ],
        "asks": [
            { "price": "0.46", "size": "150" },
            { "price": "0.47", "size": "250" }
        ],
        "min_order_size": "1",
        "tick_size": "0.01",
        "neg_risk": false,
        "last_trade_price": "0.45"
    }
    )json";

    SPSCQueue<MarketEvent, 1024> queue;

    const auto result = polymarket::parse_book_snapshot(
        payload,
        42,
        1,
        queue
    );

    REQUIRE(result.status == polymarket::SnapshotParseStatus::Ok);
    REQUIRE(result.first_sequence == 1);
    REQUIRE(result.next_sequence == 7);
    REQUIRE(result.events_emitted == 6);

    MarketEvent event{};

    REQUIRE(queue.pop(event));
    REQUIRE(event.sequence_number == 1);
    REQUIRE(event.market_id == 42);
    REQUIRE(event.type == EventType::SnapshotBegin);

    REQUIRE(queue.pop(event));
    REQUIRE(event.sequence_number == 2);
    REQUIRE(event.type == EventType::SnapshotLevel);
    REQUIRE(event.side == Side::Bid);
    REQUIRE(event.price == 4500);
    REQUIRE(event.quantity == 100 * polymarket::SIZE_SCALE);

    REQUIRE(queue.pop(event));
    REQUIRE(event.sequence_number == 3);
    REQUIRE(event.type == EventType::SnapshotLevel);
    REQUIRE(event.side == Side::Bid);
    REQUIRE(event.price == 4400);
    REQUIRE(event.quantity == 200 * polymarket::SIZE_SCALE);

    REQUIRE(queue.pop(event));
    REQUIRE(event.sequence_number == 4);
    REQUIRE(event.type == EventType::SnapshotLevel);
    REQUIRE(event.side == Side::Ask);
    REQUIRE(event.price == 4600);
    REQUIRE(event.quantity == 150 * polymarket::SIZE_SCALE);

    REQUIRE(queue.pop(event));
    REQUIRE(event.sequence_number == 5);
    REQUIRE(event.type == EventType::SnapshotLevel);
    REQUIRE(event.side == Side::Ask);
    REQUIRE(event.price == 4700);
    REQUIRE(event.quantity == 250 * polymarket::SIZE_SCALE);

    REQUIRE(queue.pop(event));
    REQUIRE(event.sequence_number == 6);
    REQUIRE(event.type == EventType::SnapshotEnd);

    REQUIRE_FALSE(queue.pop(event));
}

TEST_CASE("Polymarket snapshot events rebuild order book through BookBuilder", "[polymarket_adapter][integration]") {
    const std::string payload = R"json(
    {
        "market": "0x123",
        "asset_id": "456",
        "timestamp": "1234567890",
        "hash": "abc",
        "bids": [
            { "price": "0.44", "size": "200" },
            { "price": "0.45", "size": "100" }
        ],
        "asks": [
            { "price": "0.47", "size": "250" },
            { "price": "0.46", "size": "150" }
        ],
        "min_order_size": "1",
        "tick_size": "0.01",
        "neg_risk": false,
        "last_trade_price": "0.45"
    }
    )json";

    SPSCQueue<MarketEvent, 1024> queue;
    OrderBook book;
    BookBuilder builder(book);

    const auto parse_result = polymarket::parse_book_snapshot(
        payload,
        42,
        1,
        queue
    );

    REQUIRE(parse_result.status == polymarket::SnapshotParseStatus::Ok);

    const auto drain_result = builder.drain(queue);

    REQUIRE(drain_result.status == BookDrainStatus::Drained);
    REQUIRE(drain_result.events_consumed == 6);
    REQUIRE(drain_result.events_applied == 6);

    REQUIRE(builder.last_sequence_number() == 6);

    REQUIRE(book.get_best_bid() == 4500);
    REQUIRE(book.get_best_ask() == 4600);

    REQUIRE(book.get_volume_at_price(Side::Bid, 4500) == 100 * polymarket::SIZE_SCALE);
    REQUIRE(book.get_volume_at_price(Side::Bid, 4400) == 200 * polymarket::SIZE_SCALE);
    REQUIRE(book.get_volume_at_price(Side::Ask, 4600) == 150 * polymarket::SIZE_SCALE);
    REQUIRE(book.get_volume_at_price(Side::Ask, 4700) == 250 * polymarket::SIZE_SCALE);
}

TEST_CASE("Polymarket adapter rejects invalid JSON", "[polymarket_adapter]") {
    SPSCQueue<MarketEvent, 1024> queue;

    const auto result = polymarket::parse_book_snapshot(
        "{ bad json",
        42,
        1,
        queue
    );

    REQUIRE(result.status == polymarket::SnapshotParseStatus::InvalidJson);
    REQUIRE(result.events_emitted == 0);
}

TEST_CASE("Polymarket adapter rejects missing bid ask fields", "[polymarket_adapter]") {
    const std::string payload = R"json(
    {
        "market": "0x123",
        "asset_id": "456"
    }
    )json";

    SPSCQueue<MarketEvent, 1024> queue;

    const auto result = polymarket::parse_book_snapshot(
        payload,
        42,
        1,
        queue
    );

    REQUIRE(result.status == polymarket::SnapshotParseStatus::MissingField);
    REQUIRE(result.events_emitted == 0);
}