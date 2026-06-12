# prediction-market-arbitrage-engine

A C++20 market data and strategy evaluation engine for prediction markets. The project builds the infrastructure layer between raw market data and strategy logic: normalized event ingestion, deterministic local book reconstruction, and constraint-based arbitrage evaluation.

The current focus is deterministic market-state reconstruction and strategy evaluation, with the architecture designed to support replay, simulation, and eventual live ingestion.

---

## Architecture

Data flows through a fixed pipeline:

```
Polymarket JSON → adapter → MarketEvent → SPSC queue → BookBuilder → OrderBook → Strategy
```

**Polymarket adapter** translates raw `/book` snapshot JSON into normalized `MarketEvent` structs. It is the only place venue-specific formatting is handled. It also parses Polymarket's string-encoded decimal prices into fixed-point integers.

**MarketEvent** is the engine's internal data format. Everything becomes a `MarketEvent` before entering the pipeline. This lets the same downstream path handle snapshots, WebSocket updates, replay files, and synthetic test events without modification.

**SPSC queue** is the handoff between producer and consumer. The adapter produces events; the BookBuilder consumes them.

**BookBuilder** owns sequencing. It checks whether each incoming event is next in sequence, rejects duplicates, detects gaps, and drops unsupported event types. In the normal pipeline, the OrderBook receives events only after BookBuilder validates sequencing and event support.

**OrderBook** maintains aggregate price-level state. It stores bid and ask quantity by price level and tracks best bid/ask.

**Strategy** consumes reconstructed book state. It has no knowledge of JSON, HTTP, or anything external.

---

## Design Decisions

### Aggregate price-level book, not per-order

The book exposes `set_level(side, price, quantity)` and `clear_level(side, price)` rather than `insert_order`, `cancel_order`, and `modify_order`. Polymarket's public book data is aggregate level data — individual order IDs are not in the feed. Modeling per-order state here would mean tracking things the data does not actually tell us. The aggregate model reflects what we receive.

### Normalized events instead of direct JSON-to-book calls

The adapter does not write directly into the OrderBook. It produces `MarketEvent` structs that move through the queue and BookBuilder first. The consequence is that the engine does not need to change to support replaying recorded events, running synthetic test scenarios, or eventually handling WebSocket deltas — those are all just different sources of the same event type.

### BookBuilder owns sequencing, OrderBook owns state

Sequence validation — what is next, what is a duplicate, what is a gap — is a market data pipeline concern. Book mutation is a state concern. Mixing them makes both harder to test in isolation and harder to reason about when something goes wrong. BookBuilder handles one; OrderBook handles the other.

### Fixed-point integers for prices

Prices are stored as scaled integers: `1.0 → 10000`, `0.52 → 5200`. Floating-point arithmetic is not used on the pricing path. This avoids accumulating rounding error across operations and makes equality comparisons between price levels safe.

### Namespace for the adapter, not a virtual interface

The Polymarket adapter is a namespace of stateless translation functions (`polymarket::parse_book_snapshot`) rather than a class implementing some `IMarketDataAdapter` interface. A virtual interface makes sense when there are multiple runtime-selectable adapters. There is currently one venue. The abstraction belongs when there is a second adapter to justify it.

---

## What's Implemented

- Aggregate price-level order book with best bid/ask tracking
- `MarketEvent` model covering snapshot begin/end, level set, and level clear events
- `BookBuilder` with sequence validation, duplicate detection, and gap detection
- SPSC ring buffer for producer/consumer event handoff
- Polymarket `/book` snapshot adapter with fixed-point decimal parsing
- Constraint-based arbitrage strategy: detects implication-chain violations across related markets (e.g. `P(BTC > $100k) > P(BTC > $90k)`), computes fee-adjusted edge, and estimates executable size from top-of-book liquidity
- Catch2 unit tests covering each component in isolation and the full adapter → queue → BookBuilder → OrderBook → Strategy pipeline

The integration tests verify that components work together through the same path used by the engine, not just as isolated functions. They verify that data moves through the actual architecture end-to-end and produces correct output, not just that individual functions behave in isolation.

---

## What's Next

**Replay harness.** The immediate next step is a mechanism to record a stream of normalized `MarketEvent`s and replay them deterministically, verifying that the engine reconstructs the same book state and produces the same strategy decisions from the same input. This is the prerequisite for any meaningful backtesting or strategy evaluation over historical data.

After that:

- Paper OMS for tracking submitted orders, fills, cancels, and running position/PnL
- Benchmarks for the event pipeline with reproducible methodology
- Non-blocking network I/O for live Polymarket data ingestion
- Optional live market data ingestion after replay and simulation paths are validated

---

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

If `nlohmann/json.hpp` is missing:

```bash
mkdir -p third_party/nlohmann
curl -L https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp \
  -o third_party/nlohmann/json.hpp
```

## Tests

```bash
./build/engine_tests
```

Run by component:

```bash
./build/engine_tests "[order_book]"
./build/engine_tests "[book_builder]"
./build/engine_tests "[spsc_queue]"
./build/engine_tests "[polymarket_adapter]"
```