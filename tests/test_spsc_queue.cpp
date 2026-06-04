#include <catch2/catch_test_macros.hpp>
#include "../src/spsc_queue.hpp"
#include <thread>
#include <vector>
#include <atomic>

TEST_CASE("SPSCQueue: Single-Threaded Sanity Check", "[concurrency]") {
    SPSCQueue<int, 4> queue;
    
    REQUIRE(queue.push(10) == true);
    REQUIRE(queue.push(20) == true);
    REQUIRE(queue.push(30) == true);
    REQUIRE(queue.push(40) == true);
    REQUIRE(queue.push(50) == false); 

    int val;
    REQUIRE(queue.pop(val) == true);
    REQUIRE(val == 10);
    
    REQUIRE(queue.pop(val) == true);
    REQUIRE(val == 20);

    REQUIRE(queue.push(50) == true);
    REQUIRE(queue.push(60) == true);
}

TEST_CASE("SPSCQueue: Multi-Threaded Stress Test", "[concurrency][stress]") {
    SPSCQueue<int, 1024> queue;
    const int total_messages = 10'000'000;
    
    std::atomic<bool> producer_done{false};
    
    std::thread producer([&]() {
        for (int i = 0; i < total_messages; ++i) {
            while (!queue.push(i));
        }
        producer_done = true;
    });

    std::thread consumer([&]() {
        int expected_value = 0;
        int popped_value = 0;
        
        while (expected_value < total_messages) {
            if (queue.pop(popped_value)) {
                REQUIRE(popped_value == expected_value);
                expected_value++;
            }
        }
    });

    producer.join();
    consumer.join();
    
    REQUIRE(producer_done.load() == true);
}