#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "../src/book_builder.hpp"
#include "../src/order_book.hpp"
#include "../src/polymarket_adapter.hpp"
#include "../src/spsc_queue.hpp"

namespace {

std::string read_fixture(const std::string& path) {
    std::ifstream file(path);
    REQUIRE(file.is_open());

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

uint32_t parse_scaled_decimal(const std::string& value, uint32_t scale) {
    uint32_t whole = 0;
    uint32_t fractional = 0;
    uint32_t fractional_digits = 0;

    bool after_decimal = false;

    for (char c : value) {
        if (c == '.') {
            after_decimal = true;
            continue;
        }

        if (c < '0' || c > '9') {
            continue;
        }

        const uint32_t digit = static_cast<uint32_t>(c - '0');

        if (!after_decimal) {
            whole = whole * 10 + digit;
        } else if (fractional_digits < 8) {
            fractional = fractional * 10 + digit;
            ++fractional_digits;
        }
    }

    uint32_t fractional_scale = 1;
    for (uint32_t i = 0; i < fractional_digits; ++i) {
        fractional_scale *= 10;
    }

    return whole * scale + (fractional * scale) / fractional_scale;
}

} // namespace

TEST_CASE("Polymarket real fixture reconstructs order book", "[polymarket][fixture]") {
    constexpr uint32_t INTERNAL_MARKET_ID = 0;
    constexpr uint64_t START_SEQUENCE = 1;

    const std::string payload =
        read_fixture("tests/fixtures/polymarket/book_fed_yes.json");

    const nlohmann::json fixture = nlohmann::json::parse(payload);

    REQUIRE(fixture.contains("bids"));
    REQUIRE(fixture.contains("asks"));
    REQUIRE_FALSE(fixture["bids"].empty());
    REQUIRE_FALSE(fixture["asks"].empty());

    uint32_t expected_best_bid = 0;
    uint32_t expected_best_ask = UINT32_MAX;

    for (const auto& bid : fixture["bids"]) {
        const uint32_t price = parse_scaled_decimal(
            bid.at("price").get<std::string>(),
            polymarket::PRICE_SCALE
        );

        expected_best_bid = std::max(expected_best_bid, price);
    }

    for (const auto& ask : fixture["asks"]) {
        const uint32_t price = parse_scaled_decimal(
            ask.at("price").get<std::string>(),
            polymarket::PRICE_SCALE
        );

        expected_best_ask = std::min(expected_best_ask, price);
    }

    SPSCQueue<MarketEvent, 1024> queue;
    OrderBook book;
    BookBuilder builder(book);

    const auto parse_result = polymarket::parse_book_snapshot(
        payload,
        INTERNAL_MARKET_ID,
        START_SEQUENCE,
        queue
    );

    REQUIRE(parse_result.status == polymarket::SnapshotParseStatus::Ok);
    REQUIRE(parse_result.events_emitted > 0);

    const auto drain_result = builder.drain(queue);

    REQUIRE(drain_result.status == BookDrainStatus::Drained);
    REQUIRE(drain_result.events_consumed == parse_result.events_emitted);
    REQUIRE(drain_result.events_applied == parse_result.events_emitted);

    REQUIRE(book.get_best_bid() == static_cast<int32_t>(expected_best_bid));
    REQUIRE(book.get_best_ask() == static_cast<int32_t>(expected_best_ask));

    REQUIRE(book.get_volume_at_price(Side::Bid, expected_best_bid) > 0);
    REQUIRE(book.get_volume_at_price(Side::Ask, expected_best_ask) > 0);
}