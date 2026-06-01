#include <iostream>
#include <chrono>
#include <array>
#include "strategy.hpp"
#include "order_book.hpp"

int main() {
    // 1. Initialize core components
    OrderBook strong_book(100);
    OrderBook weak_book(100);
    Strategy engine;

    // Cold Path configuration
    engine.configure_market(0, 9000, 2, &strong_book); // 2 bps fee
    engine.configure_market(1, 10000, 2, &weak_book);
    engine.configure_constraint(0, 1); // Strong implies weak

    // 2. Seed the books ONCE outside the loop (Occupies exactly 2 slots in the pool)
    strong_book.insert_order(1, 'B', 600, 50); // Bid $0.60
    weak_book.insert_order(2, 'A', 590, 100);  // Ask $0.59 -> Arbitrage exists!

    std::array<Opportunity, 10> output_buffer{};
    const size_t ITERATIONS = 1'000'000;

    std::cout << "Running clean microbenchmark for " << ITERATIONS << " iterations..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    uint64_t total_opportunities_found = 0;
    for (size_t i = 0; i < ITERATIONS; ++i) {
        
        uint32_t found = engine.evaluate_channels(output_buffer.data(), output_buffer.size());
        total_opportunities_found += found;

        // HFT Optimization Guard: This inline assembly tells the compiler:
        // "Treat output_buffer as dirty/modified memory." 
        // This prevents the compiler from optimizing out the loop during -O3 compilation.
        asm volatile("" : : "g"(output_buffer.data()) : "memory");
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    double avg_latency = static_cast<double>(duration) / ITERATIONS;
    double throughput = (ITERATIONS / (static_cast<double>(duration) / 1'000'000'000.0)) / 1'000'000.0;

    std::cout << "--------- BENCHMARK RESULTS ---------" << std::endl;
    std::cout << "Total Time Taken: " << duration / 1'000'000.0 << " ms" << std::endl;
    std::cout << "Average Latency Per Loop: " << avg_latency << " ns" << std::endl;
    std::cout << "Throughput: " << throughput << " Million updates/sec" << std::endl;
    std::cout << "Total Opps Processed: " << total_opportunities_found << std::endl;

    return 0;
}