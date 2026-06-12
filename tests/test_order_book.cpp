#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "order_book.hpp"

TEST_CASE("OrderBook starts empty", "[order_book]") {
    OrderBook book;

    REQUIRE(book.get_best_bid() == -1);
    REQUIRE(book.get_best_ask() == -1);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 0);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5000) == 0);
}

TEST_CASE("OrderBook sets bid levels and tracks best bid", "[order_book][levels]") {
    OrderBook book;

    book.set_level(Side::Bid, 5000, 10);
    REQUIRE(book.get_best_bid() == 5000);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 10);

    book.set_level(Side::Bid, 5100, 20);
    REQUIRE(book.get_best_bid() == 5100);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5100) == 20);

    book.set_level(Side::Bid, 4900, 30);
    REQUIRE(book.get_best_bid() == 5100);
    REQUIRE(book.get_volume_at_price(Side::Bid, 4900) == 30);
}

TEST_CASE("OrderBook sets ask levels and tracks best ask", "[order_book][levels]") {
    OrderBook book;

    book.set_level(Side::Ask, 5100, 10);
    REQUIRE(book.get_best_ask() == 5100);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 10);

    book.set_level(Side::Ask, 5050, 20);
    REQUIRE(book.get_best_ask() == 5050);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5050) == 20);

    book.set_level(Side::Ask, 5200, 30);
    REQUIRE(book.get_best_ask() == 5050);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5200) == 30);
}

TEST_CASE("OrderBook overwrites aggregate volume at existing level", "[order_book][levels]") {
    OrderBook book;

    book.set_level(Side::Bid, 5000, 10);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 10);

    book.set_level(Side::Bid, 5000, 25);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 25);
    REQUIRE(book.get_best_bid() == 5000);

    book.set_level(Side::Ask, 5100, 20);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 20);

    book.set_level(Side::Ask, 5100, 5);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 5);
    REQUIRE(book.get_best_ask() == 5100);
}

TEST_CASE("OrderBook clear_level removes non-best level without changing best", "[order_book][levels]") {
    OrderBook book;

    book.set_level(Side::Bid, 5000, 10);
    book.set_level(Side::Bid, 5100, 20);
    book.set_level(Side::Bid, 4900, 30);

    REQUIRE(book.get_best_bid() == 5100);

    book.clear_level(Side::Bid, 5000);

    REQUIRE(book.get_best_bid() == 5100);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 0);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5100) == 20);
    REQUIRE(book.get_volume_at_price(Side::Bid, 4900) == 30);
}

TEST_CASE("OrderBook clear_level recomputes best bid", "[order_book][levels]") {
    OrderBook book;

    book.set_level(Side::Bid, 5000, 10);
    book.set_level(Side::Bid, 5100, 20);
    book.set_level(Side::Bid, 4900, 30);

    REQUIRE(book.get_best_bid() == 5100);

    book.clear_level(Side::Bid, 5100);
    REQUIRE(book.get_best_bid() == 5000);

    book.clear_level(Side::Bid, 5000);
    REQUIRE(book.get_best_bid() == 4900);

    book.clear_level(Side::Bid, 4900);
    REQUIRE(book.get_best_bid() == -1);
}

TEST_CASE("OrderBook clear_level recomputes best ask", "[order_book][levels]") {
    OrderBook book;

    book.set_level(Side::Ask, 5100, 10);
    book.set_level(Side::Ask, 5050, 20);
    book.set_level(Side::Ask, 5200, 30);

    REQUIRE(book.get_best_ask() == 5050);

    book.clear_level(Side::Ask, 5050);
    REQUIRE(book.get_best_ask() == 5100);

    book.clear_level(Side::Ask, 5100);
    REQUIRE(book.get_best_ask() == 5200);

    book.clear_level(Side::Ask, 5200);
    REQUIRE(book.get_best_ask() == -1);
}

TEST_CASE("OrderBook treats zero quantity set_level as clear", "[order_book][levels]") {
    OrderBook book;

    book.set_level(Side::Bid, 5000, 10);
    REQUIRE(book.get_best_bid() == 5000);

    book.set_level(Side::Bid, 5000, 0);

    REQUIRE(book.get_best_bid() == -1);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 0);

    book.set_level(Side::Ask, 5100, 20);
    REQUIRE(book.get_best_ask() == 5100);

    book.set_level(Side::Ask, 5100, 0);

    REQUIRE(book.get_best_ask() == -1);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 0);
}

TEST_CASE("OrderBook clear resets all levels and inside market", "[order_book][levels]") {
    OrderBook book;

    book.set_level(Side::Bid, 5000, 10);
    book.set_level(Side::Bid, 5100, 20);
    book.set_level(Side::Ask, 5050, 30);
    book.set_level(Side::Ask, 5200, 40);

    REQUIRE(book.get_best_bid() == 5100);
    REQUIRE(book.get_best_ask() == 5050);

    book.clear();

    REQUIRE(book.get_best_bid() == -1);
    REQUIRE(book.get_best_ask() == -1);

    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 0);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5100) == 0);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5050) == 0);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5200) == 0);
}

TEST_CASE("OrderBook rejects out-of-bounds prices", "[order_book][safety]") {
    OrderBook book;

    REQUIRE_THROWS_AS(
        book.set_level(Side::Bid, MAX_PRICE_POINTS, 10),
        std::runtime_error
    );

    REQUIRE_THROWS_AS(
        book.set_level(Side::Ask, MAX_PRICE_POINTS + 1, 10),
        std::runtime_error
    );
}

TEST_CASE("OrderBook clear_level on empty level is safe", "[order_book][safety]") {
    OrderBook book;

    REQUIRE_NOTHROW(book.clear_level(Side::Bid, 5000));
    REQUIRE_NOTHROW(book.clear_level(Side::Ask, 5100));

    REQUIRE(book.get_best_bid() == -1);
    REQUIRE(book.get_best_ask() == -1);
    REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 0);
    REQUIRE(book.get_volume_at_price(Side::Ask, 5100) == 0);
}