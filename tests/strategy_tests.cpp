#include <catch2/catch_test_macros.hpp>
#include <array>
#include "../src/strategy.hpp"
#include "../src/order_book.hpp" 

TEST_CASE("Strategy validates implication arbitrage and calculates max_size", "[strategy]") {
    // 1. Setup the physical order books
    OrderBook book_90k;
    OrderBook book_100k;

    // Market A (90k): Someone is panicking and willing to SELL the 90k YES contract very cheap at $0.50.
    // They are offering 100 contracts. (Remember: tick size math, so 500 = $0.500)
    book_90k.insert_order(1, 'A', 500, 100);

    // Market B (100k): Someone is overly optimistic and willing to BUY the 100k YES contract at $0.60.
    // They want to buy 40 contracts. (600 = $0.600)
    book_100k.insert_order(2, 'B', 600, 40);

    // 2. Configure the Strategy Engine (The Cold Path)
    Strategy engine;
    
    // Internal Index 0 -> Market 90k (ID 9000, 0 fee for testing)
    engine.configure_market(0, 9000, 0, &book_90k);
    
    // Internal Index 1 -> Market 100k (ID 10000, 0 fee for testing)
    engine.configure_market(1, 10000, 0, &book_100k);

    // Constraint: 100k (Index 1) implies 90k (Index 0)
    // Therefore, Price(90k) MUST BE >= Price(100k).
    engine.configure_constraint(1, 0); 

    // 3. Execution (The Hot Path)
    // Pre-allocate our stack buffer for the engine to write into
    std::array<Opportunity, 10> output_buffer{};
    
    size_t opportunities_found = engine.evaluate_channels(output_buffer.data(), output_buffer.size());

    // 4. Verify the math and logic
    REQUIRE(opportunities_found == 1);
    
    const auto& opp = output_buffer[0];
    
    // Did it map the external IDs correctly?
    REQUIRE(opp.buy_market_id == 9000);   // We buy the cheap 90k
    REQUIRE(opp.sell_market_id == 10000); // We sell the expensive 100k
    
    // Did it calculate edges correctly?
    REQUIRE(opp.buy_price == 500);
    REQUIRE(opp.sell_price == 600);
    REQUIRE(opp.gross_edge == 100); // 600 - 500
    
    // **The Bottleneck Check**
    // 90k has 100 volume. 100k has 40 volume. We can only trade 40 safely.
    REQUIRE(opp.max_size == 40);
}

TEST_CASE("Strategy respects output buffer boundaries", "[strategy]") {
    OrderBook book_90k, book_100k;
    book_90k.insert_order(1, 'A', 500, 100);
    book_100k.insert_order(2, 'B', 600, 40);

    Strategy engine;
    engine.configure_market(0, 9000, 0, &book_90k);
    engine.configure_market(1, 10000, 0, &book_100k);
    engine.configure_constraint(1, 0);

    // Intentionally pass a buffer with 0 capacity
    std::array<Opportunity, 0> tiny_buffer{};
    size_t found = engine.evaluate_channels(tiny_buffer.data(), tiny_buffer.size());

    // The engine should detect the limit and abort safely, preventing a segfault buffer overrun
    REQUIRE(found == 0); 
}