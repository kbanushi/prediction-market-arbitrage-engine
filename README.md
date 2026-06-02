# Prediction Market Arbitrage Engine

A C++20 trading infrastructure project for prediction-market arbitrage, focused on building a fast, deterministic order book and strategy evaluation pipeline. The engine currently emphasizes cache-conscious data layout, preallocated storage, constant-time book mutations, and reproducible hot-path benchmarks.

## Overview

This project is a work-in-progress implementation of a miniature prediction-market trading engine. The current system includes:

- A preallocated limit order book
- Fixed price-level arrays
- Intrusive index-based linked lists for per-price order queues
- Dense order ID lookup
- Constraint-based arbitrage evaluation
- Catch2 unit tests
- Synthetic hot-path microbenchmarks

The goal is to build a realistic trading-system foundation before adding more advanced components such as market data replay, OMS/order state management, concurrency, and non-blocking network I/O.

## Architecture Highlights

### Preallocated Hot Path

The order book uses a `std::vector`-backed arena and recycles order slots through an internal free list. Once initialized with sufficient capacity, add/cancel/modify operations avoid `new`/`delete` allocations on the hot path.

### Cache-Conscious Book Layout

The book avoids node-heavy associative containers for price levels. Instead, it uses fixed-size price arrays and index-based linked lists to keep core book state compact and predictable in memory.

Order ID lookup uses `ankerl::unordered_dense`, which provides a faster, more cache-friendly hash map than `std::unordered_map` in the current benchmark workload.

### Constant-Time Book Mutations

Critical book operations are designed around average-case constant-time behavior:

- Add order
- Cancel order by ID
- Modify order quantity
- Price-level volume lookup
- Best bid/ask tracking

Order cancellation and modification derive side and price from internal order state instead of trusting caller-provided metadata. This improves correctness by making the order book the source of truth for existing order metadata.

### Order Struct and API Refactor

The order book API was refactored so existing orders are cancelled and modified by `OrderId` instead of requiring callers to pass redundant side/price metadata.

Before:

```cpp
cancel_order(order_id, side, price);
modify_order(order_id, side, price, new_quantity);
```

After:

```cpp
cancel_order(order_id);
modify_order(order_id, new_quantity);
```

The `Order` struct now stores the side and price needed for book mutation. This reduces caller-side state duplication, prevents accidental mutation of the wrong price level, and reduced average hot-path latency by approximately 5 ns in local synthetic benchmarks.

### Constraint-Based Arbitrage Evaluation

The current strategy layer evaluates cross-market prediction constraints. It compares implied probabilities across related markets, estimates gross edge, subtracts fees, and computes executable size based on available top-of-book liquidity.

## Current Status and Roadmap

This project is in active development. The core order book, memory-pool structure, basic strategy evaluation, tests, and synthetic benchmarks are implemented.

- [x] Phase 1: Core order book with preallocated storage, fixed price levels, dense ID lookup, and unit tests
- [x] Phase 2: Constraint-based arbitrage evaluator with fee-adjusted edge calculation
- [ ] Phase 3: Paper OMS for tracking submitted orders, fills, cancels, rejects, positions, and PnL
- [ ] Phase 4: Market data replay engine for deterministic event-driven backtesting
- [ ] Phase 5: SPSC ring buffers for inter-thread communication
- [ ] Phase 6: Non-blocking network I/O for market data ingestion
- [ ] Phase 7: Expanded benchmarks and profiling reports

## Hot-Path Microbenchmarks

Benchmarks were run locally on Apple Silicon M2 using Clang with release-mode optimization.

The benchmark initializes a synthetic order book with 20,000 active orders and executes 1,000,000 randomized order modifications using a lightweight Linear Congruential Generator.

| Data Structure Strategy | Avg Latency per Update | Throughput |
| :--- | :--- | :--- |
| Dense map + memory pool + internal order metadata refactor | ~37 ns | ~26.8M updates/sec |
| Dense map + preallocated memory pool | ~41 ns | ~24.3M updates/sec |
| `std::unordered_map` baseline | ~67 ns | ~14.8M updates/sec |

In this synthetic workload, the dense-map/preallocated design reduced average update latency by roughly 39% compared with the `std::unordered_map` baseline.

A later order-book API refactor, which moved side/price ownership into the internal `Order` struct and simplified cancel/modify calls to operate by `OrderId`, further reduced average hot-path latency by approximately 5 ns in local synthetic benchmarks.

> Note: These are local synthetic microbenchmarks, not production exchange benchmarks. Results may vary by machine, compiler, workload, and build configuration.

## Build Instructions

### Clone the repository

```bash
git clone https://github.com/kbanushi/prediction-market-arbitrage-engine.git
cd prediction-market-arbitrage-engine
```

### Fetch external dependencies

This project currently uses `ankerl::unordered_dense` for dense hash-map storage and Catch2 for unit tests.

```bash
curl -L -o src/ankerl_unordered_dense.hpp \
  https://raw.githubusercontent.com/martinus/unordered_dense/main/include/ankerl/unordered_dense.h

curl -L -o src/stl.h \
  https://raw.githubusercontent.com/martinus/unordered_dense/main/include/ankerl/stl.h
```

Catch2 is pulled through CMake `FetchContent`.

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Run tests

```bash
ctest --test-dir build --output-on-failure
```

### Run benchmark

```bash
./build/PredictionMarketEngine
```

## Testing

The test suite currently covers:

- Order insertion and cancellation
- Best bid/ask tracking
- Price-level volume aggregation
- Memory pool exhaustion and recycling
- Safe handling of invalid/phantom cancels
- Strategy edge detection
- Fee-adjusted arbitrage calculations
- Output buffer bounds

## Project Direction

The next major milestone is adding a paper OMS. This will separate public market state from internal order state and allow the engine to track submitted orders, acknowledgements, partial fills, cancels, rejects, positions, and realized/unrealized PnL.

Longer term, the project is intended to evolve into a deterministic event-driven trading simulator with market data replay, strategy evaluation, risk checks, and benchmarkable execution paths.