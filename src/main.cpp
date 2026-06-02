#include <iostream>
#include <chrono>
#include <vector>
#include <numeric>
#include "strategy.hpp"
#include "order_book.hpp"

// Fast, deterministic pseudo-random number generator to avoid std::mt19937 overhead
__attribute__((always_inline)) inline uint32_t fast_rand(uint32_t& state) {
    state = state * 1664525 + 1013904223;
    return state;
}

int main() {
    OrderBook strong_book(65536);
    OrderBook weak_book(65536);
    Strategy engine;

    engine.configure_market(0, 9000, 2, &strong_book); 
    engine.configure_market(1, 10000, 2, &weak_book);
    engine.configure_constraint(0, 1); 

    constexpr size_t INITIAL_ORDERS = 20000;
    constexpr size_t ITERATIONS = 1000000;
    
    std::cout << "--- HOT-PATH MICROBENCHMARK ---\n";
    std::cout << "Pre-populating book with " << INITIAL_ORDERS << " unique active orders...\n";

    for (uint32_t i = 1; i <= INITIAL_ORDERS; ++i) {
        char side = (i % 2 == 0) ? 'B' : 'A';
        int32_t base_px = (side == 'B') ? 590 : 610;
        strong_book.insert_order(i, side, base_px, 10);
        weak_book.insert_order(i + INITIAL_ORDERS, side, base_px, 10);
    }

    std::array<Opportunity, 10> output_buffer{};
    uint32_t rand_state = 42;

    std::cout << "Executing " << ITERATIONS << " random hot-path updates across state space...\n";

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < ITERATIONS; ++i) {
        uint32_t target_id = (fast_rand(rand_state) % INITIAL_ORDERS) + 1;
        int32_t dynamic_px = 600 + (fast_rand(rand_state) % 20) - 10;
        uint32_t dynamic_sz = 5 + (fast_rand(rand_state) % 95);

        strong_book.modify_order(target_id, dynamic_px, dynamic_sz);

        engine.evaluate_channels(output_buffer.data(), output_buffer.size());
    }

    auto end = std::chrono::high_resolution_clock::now();
    
    double total_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double avg_latency_ns = (total_time_ms * 1000000.0) / ITERATIONS;
    double throughput = ITERATIONS / (total_time_ms / 1000.0) / 1000000.0;

    std::cout << "--- HOT-PATH MICROBENCHMARK ---\n";
    std::cout << "Total Time Taken: " << total_time_ms << " ms" << std::endl;
    std::cout << "Average Latency Per Loop: " << avg_latency_ns << " ns" << std::endl;
    std::cout << "Throughput: " << throughput << " Million updates/sec" << std::endl;

    return 0;
}