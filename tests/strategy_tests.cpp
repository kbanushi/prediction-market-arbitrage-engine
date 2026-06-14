#include <catch2/catch_test_macros.hpp>

#include <array>

#include "../src/strategy.hpp"
#include "../src/order_book.hpp"

TEST_CASE("Strategy validates implication arbitrage and calculates max_size", "[strategy]") {
    OrderBook book_90k;
    OrderBook book_100k;

    book_90k.set_level(Side::Ask, 500, 100);
    book_100k.set_level(Side::Bid, 600, 40);

    Strategy engine;

    engine.configure_market(0, 0, &book_90k);
    engine.configure_market(1, 0, &book_100k);
    engine.configure_constraint(1, 0);

    std::array<Opportunity, 10> output_buffer{};

    const uint32_t opportunities_found = engine.evaluate_channels(
        output_buffer.data(),
        static_cast<uint32_t>(output_buffer.size())
    );

    REQUIRE(opportunities_found == 1);

    const auto& opp = output_buffer[0];

    REQUIRE(opp.buy_market_id == 0);
    REQUIRE(opp.sell_market_id == 1);

    REQUIRE(opp.buy_price == 500);
    REQUIRE(opp.sell_price == 600);
    REQUIRE(opp.gross_edge == 100);
    REQUIRE(opp.net_edge == 100);
    REQUIRE(opp.estimated_fees == 0);
    REQUIRE(opp.max_size == 40);
}

TEST_CASE("Strategy respects output buffer boundaries", "[strategy]") {
    OrderBook book_90k;
    OrderBook book_100k;

    book_90k.set_level(Side::Ask, 500, 100);
    book_100k.set_level(Side::Bid, 600, 40);

    Strategy engine;

    engine.configure_market(0, 0, &book_90k);
    engine.configure_market(1, 0, &book_100k);
    engine.configure_constraint(1, 0);

    std::array<Opportunity, 0> tiny_buffer{};

    const uint32_t found = engine.evaluate_channels(
        tiny_buffer.data(),
        static_cast<uint32_t>(tiny_buffer.size())
    );

    REQUIRE(found == 0);
}