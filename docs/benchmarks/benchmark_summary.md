### Benchmarks

Benchmarks were run in Release mode with Google Benchmark. The fixture reconstruction benchmark includes JSON parsing from a recorded Polymarket CLOB order book. The synthetic full-pipeline benchmark excludes JSON parsing and uses pre-generated market-data events.

| Benchmark | Mean | Median | Stddev | CV | Notes |
|---|---:|---:|---:|---:|---|
| `BM_PolymarketFixture_ReconstructBook` | 53.06 µs | 52.98 µs | 294.82 ns | 0.56% | Recorded Polymarket JSON fixture → MarketEvent queue → BookBuilder → OrderBook |
| `BM_TradingLoop_StaticOpportunityToPaperOrders` | 1.19 µs | 1.19 µs | 9.03 ns | 0.76% | Static reconstructed books → Strategy → IntentBuilder → RiskManager → PaperOMS |
| `BM_SyntheticUpdates_FullPipeline` | 79.43 µs | 79.40 µs | 98.60 ns | 0.12% | Pre-generated updates → BookBuilder → OrderBook → Strategy → RiskManager → PaperOMS |

> Results are machine-dependent and should be interpreted as relative project measurements, not exchange-colocation latency claims.
