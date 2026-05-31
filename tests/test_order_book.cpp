#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include "order_book.hpp"

// ---------------------------------------------------------
// TEST SUITE 1: Memory Pool & Allocation Limits
// ---------------------------------------------------------
TEST_CASE("Memory Pool Constraints and Recycling", "[memory]") {
    // Initialize a remarkably small book to force allocation boundaries
    OrderBook book(3); 

    SECTION("Exhausting the pool throws an exception") {
        REQUIRE_NOTHROW(book.insert_order(1, 'B', 100, 10));
        REQUIRE_NOTHROW(book.insert_order(2, 'A', 105, 10));
        REQUIRE_NOTHROW(book.insert_order(3, 'B', 99, 10));

        // The 4th order must crash the pool
        REQUIRE_THROWS_AS(book.insert_order(4, 'B', 98, 10), std::runtime_error);
    }

    SECTION("Freed memory slots are instantly recycled") {
        book.insert_order(1, 'B', 100, 10);
        book.insert_order(2, 'A', 105, 10);
        book.insert_order(3, 'B', 99, 10);

        // Cancel order 2, freeing exactly one slot back to the free_head
        book.cancel_order(2, 'A', 105);

        // We should now be able to insert exactly one more order
        REQUIRE_NOTHROW(book.insert_order(4, 'B', 98, 15));
        
        // But a 5th should still fail
        REQUIRE_THROWS_AS(book.insert_order(5, 'A', 106, 10), std::runtime_error);
    }
}

// ---------------------------------------------------------
// TEST SUITE 2: Price Tracking & Spread Updates
// ---------------------------------------------------------
TEST_CASE("Inside Market (Best Bid/Ask) Tracking", "[pricing]") {
    OrderBook book(100);

    SECTION("Ascending bids correctly update the best bid") {
        book.insert_order(1, 'B', 5000, 10);
        REQUIRE(book.get_best_bid() == 5000);

        book.insert_order(2, 'B', 5100, 10);
        REQUIRE(book.get_best_bid() == 5100);

        // A lower bid shouldn't change the best bid
        book.insert_order(3, 'B', 4900, 10);
        REQUIRE(book.get_best_bid() == 5100);
    }

    SECTION("Canceling the best bid forces a downward scan") {
        book.insert_order(1, 'B', 5000, 10);
        book.insert_order(2, 'B', 5100, 10);
        book.insert_order(3, 'B', 5200, 10);

        REQUIRE(book.get_best_bid() == 5200);

        // Wipe out the top of the book
        book.cancel_order(3, 'B', 5200);
        REQUIRE(book.get_best_bid() == 5100);

        // Wipe out the middle (should not affect best bid)
        book.cancel_order(1, 'B', 5000);
        REQUIRE(book.get_best_bid() == 5100);

        // Wipe out the final bid (should reset to -1)
        book.cancel_order(2, 'B', 5100);
        REQUIRE(book.get_best_bid() == -1);
    }
}

// ---------------------------------------------------------
// TEST SUITE 3: Volume & Linked List Splicing
// ---------------------------------------------------------
TEST_CASE("Queue Volume and Mid-List Splicing", "[queue]") {
    OrderBook book(100);

    SECTION("Volume aggregates correctly across identical prices") {
        book.insert_order(1, 'B', 5000, 15);
        book.insert_order(2, 'B', 5000, 25);
        
        REQUIRE(book.get_volume_at_price('B', 5000) == 40);

        // Cancel the HEAD of the queue
        book.cancel_order(1, 'B', 5000);
        
        // Volume should update, and the price level should survive
        REQUIRE(book.get_volume_at_price('B', 5000) == 25);
        REQUIRE(book.get_best_bid() == 5000);
    }
}

// ---------------------------------------------------------
// TEST SUITE 4: Hardware Boundaries & Safety
// ---------------------------------------------------------
TEST_CASE("Array Bounds and Silent Errors", "[safety]") {
    // Assuming MAX_PRICE_POINTS is defined (e.g., 10000)
    OrderBook book(100);

    SECTION("Out of bounds prices trigger immediate throws") {
        // Attempting to index outside our static array is catastrophic
        REQUIRE_THROWS_AS(book.insert_order(1, 'B', 10000, 10), std::runtime_error);
        REQUIRE_THROWS_AS(book.insert_order(2, 'A', 99999, 10), std::runtime_error);
    }

    SECTION("Phantom cancels are silently ignored") {
        book.insert_order(1, 'B', 5000, 10);
        
        // Canceling an order that never existed should not segfault
        REQUIRE_NOTHROW(book.cancel_order(999, 'B', 5000));
        
        // Canceling an order that was ALREADY canceled should not segfault
        book.cancel_order(1, 'B', 5000);
        REQUIRE_NOTHROW(book.cancel_order(1, 'B', 5000));
    }
}