#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include "order_book.hpp"

TEST_CASE("Memory Pool Constraints and Recycling", "[memory]") {
    OrderBook book(3); 

    SECTION("Exhausting the pool throws an exception") {
        REQUIRE_NOTHROW(book.insert_order(1, Side::Bid, 100, 10));
        REQUIRE_NOTHROW(book.insert_order(2, Side::Ask, 105, 10));
        REQUIRE_NOTHROW(book.insert_order(3, Side::Bid, 99, 10));

        REQUIRE_THROWS_AS(book.insert_order(4, Side::Bid, 98, 10), std::runtime_error);
    }

    SECTION("Freed memory slots are instantly recycled") {
        book.insert_order(1, Side::Bid, 100, 10);
        book.insert_order(2, Side::Ask, 105, 10);
        book.insert_order(3, Side::Bid, 99, 10);

        book.cancel_order(2);

        REQUIRE_NOTHROW(book.insert_order(4, Side::Bid, 98, 15));
        
        REQUIRE_THROWS_AS(book.insert_order(5, Side::Ask, 106, 10), std::runtime_error);
    }
}

TEST_CASE("Inside Market (Best Bid/Ask) Tracking", "[pricing]") {
    OrderBook book(100);

    SECTION("Ascending bids correctly update the best bid") {
        book.insert_order(1, Side::Bid, 5000, 10);
        REQUIRE(book.get_best_bid() == 5000);

        book.insert_order(2, Side::Bid, 5100, 10);
        REQUIRE(book.get_best_bid() == 5100);

        book.insert_order(3, Side::Bid, 4900, 10);
        REQUIRE(book.get_best_bid() == 5100);
    }

    SECTION("Canceling the best bid forces a downward scan") {
        book.insert_order(1, Side::Bid, 5000, 10);
        book.insert_order(2, Side::Bid, 5100, 10);
        book.insert_order(3, Side::Bid, 5200, 10);

        REQUIRE(book.get_best_bid() == 5200);

        book.cancel_order(3);
        REQUIRE(book.get_best_bid() == 5100);

        book.cancel_order(1);
        REQUIRE(book.get_best_bid() == 5100);

        book.cancel_order(2);
        REQUIRE(book.get_best_bid() == -1);
    }
}


TEST_CASE("Queue Volume and Mid-List Splicing", "[queue]") {
    OrderBook book(100);

    SECTION("Volume aggregates correctly across identical prices") {
        book.insert_order(1, Side::Bid, 5000, 15);
        book.insert_order(2, Side::Bid, 5000, 25);
        
        REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 40);

        book.cancel_order(1);
        
        REQUIRE(book.get_volume_at_price(Side::Bid, 5000) == 25);
        REQUIRE(book.get_best_bid() == 5000);
    }
}


TEST_CASE("Array Bounds and Silent Errors", "[safety]") {
    OrderBook book(100);

    SECTION("Out of bounds prices trigger immediate throws") {
        REQUIRE_THROWS_AS(book.insert_order(1, Side::Bid, 10000, 10), std::runtime_error);
        REQUIRE_THROWS_AS(book.insert_order(2, Side::Ask, 99999, 10), std::runtime_error);
    }

    SECTION("Phantom cancels are silently ignored") {
        book.insert_order(1, Side::Bid, 5000, 10);
        
        REQUIRE_NOTHROW(book.cancel_order(999));
        
        book.cancel_order(1);
        REQUIRE_NOTHROW(book.cancel_order(1));
    }
}