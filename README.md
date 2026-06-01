# Prediction Market Trading Engine

A low-latency, deterministic order book and matching engine designed for high-frequency prediction market arbitrage. Built in C++20, this engine focuses on strict hardware cache-line optimization and zero-allocation hot paths.

## Architecture Highlights

* **Zero-Allocation Hot Path:** Built around a custom intrusive memory pool (`std::vector`-backed arena) combined with index-based linked lists for price levels. This guarantees zero `new`/`delete` heap allocations during live trading execution.
* **Cache-Locality Optimization:** Replaced standard node-chained associative containers (`std::unordered_map`) with contiguous flat maps (`ankerl::unordered_dense`). This prevents heap fragmentation, maximizes L2/L3 hardware prefetching, and eliminates pointer-chasing stalls.
* **$O(1)$ Time Complexity:** All critical path operations (inserts, cancels, modifies, and price-level volume lookups) execute in constant time regardless of market depth.

## Current Status and Roadmap 
This project is in active development. The core memory architecture and state-space execution paths are complete. Current development is focused on the inter-thread concurrency layer.
- [x] Phase 1: Core Engine: O(1) Order Book, Intrusive Memory Pool, Cache-Aligned Maps.
- [ ] Phase 2: Concurrency Barrier: Lock-free, atomic Single-Producer Single-Consumer (SPSC) ring buffers for inter-thread communication.
- [ ] Phase 3: Network I/O: Asynchronous epoll/kqueue event loop for non-blocking market data ingestion.
- [ ] Phase 4: Quantitative Execution: Real-time cross-market probability arbitrage logic.

## Hot-Path Microbenchmarks

*Note: Benchmarks run on Apple Silicon M2, compiled with Clang `O3`.*

The microbenchmark simulates an adversarial, high-volatility environment by pre-allocating a state space of **20,000 active concurrent orders** and executing **1,000,000 random modifications** using a custom Linear Congruential Generator (LCG) to force cache-miss scenarios.

| Data Structure Strategy | Avg Latency per Update | Throughput (Updates/sec) |
| :--- | :--- | :--- |
| Contiguous Flat Map + Memory Pool | ~41 ns | ~24.3 Million |
| Standard Library (`std::unordered_map`) | ~67 ns | ~14.8 Million |

*Result: A 63% latency reduction strictly through optimized memory layout and cache alignment.*

## Dependencies & Setup

This project relies on two external libraries for testing and optimized map layouts. Clone the repository and pull the dependencies into your project structure:

```bash
# Clone this repository
git clone [https://github.com/yourusername/PredictionMarketEngine.git](https://github.com/yourusername/PredictionMarketEngine.git)
cd PredictionMarketEngine

# Clone required external libraries into your include/third-party paths
curl -L -o src/ankerl_unordered_dense.hpp https://raw.githubusercontent.com/martinus/unordered_dense/main/include/ankerl/unordered_dense.h
curl -L -o src/stl.h https://raw.githubusercontent.com/martinus/unordered_dense/main/include/ankerl/stl.h

# 2. Catch2 Testing Framework (For the unit testing suite)
git clone [https://github.com/catchorg/Catch2.git](https://github.com/catchorg/Catch2.git)

## Build Instructions:
# Generate build files
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Compile the engine
cmake --build build

# Run the hot-path microbenchmark
./build/PredictionMarketEngine

## Executing tests:
cd build
ctest --output-on-failure
