#include <catch2/catch_test_macros.hpp>
#include "../src/spsc_queue.hpp" // Adjust path as needed
#include <thread>
#include <vector>
#include <atomic>

TEST_CASE("SPSCQueue: Single-Threaded Sanity Check", "[concurrency]") {
    SPSCQueue<int, 4> queue; // Tiny capacity of 4 to force bitwise wrapping
    
    REQUIRE(queue.push(10) == true);
    REQUIRE(queue.push(20) == true);
    REQUIRE(queue.push(30) == true);
    REQUIRE(queue.push(40) == true);
    
    // The queue is now full (Capacity 4)
    REQUIRE(queue.push(50) == false); 

    int val;
    REQUIRE(queue.pop(val) == true);
    REQUIRE(val == 10);
    
    REQUIRE(queue.pop(val) == true);
    REQUIRE(val == 20);

    // After popping two, we should be able to push two more (wrapping around)
    REQUIRE(queue.push(50) == true);
    REQUIRE(queue.push(60) == true);
}

TEST_CASE("SPSCQueue: Multi-Threaded Stress Test", "[concurrency][stress]") {
    // 1024 slots, testing with 10 Million sequential integers
    SPSCQueue<int, 1024> queue;
    const int total_messages = 10'000'000;
    
    std::atomic<bool> producer_done{false};
    
    // PRODUCER THREAD
    std::thread producer([&]() {
        for (int i = 0; i < total_messages; ++i) {
            // Spin-wait until the queue has space
            while (!queue.push(i)) {
                // In a real engine, we'd pause or spin optimally. 
                // Here, we just hammer the CPU.
            }
        }
        producer_done = true;
    });

    // CONSUMER THREAD
    std::thread consumer([&]() {
        int expected_value = 0;
        int popped_value = 0;
        
        while (expected_value < total_messages) {
            if (queue.pop(popped_value)) {
                // If we pop a value, it MUST strictly equal the expected sequence.
                // If the memory barrier failed, we might read garbage or skip numbers.
                REQUIRE(popped_value == expected_value);
                expected_value++;
            }
        }
    });

    producer.join();
    consumer.join();
    
    // Final sanity check
    REQUIRE(producer_done.load() == true);
}