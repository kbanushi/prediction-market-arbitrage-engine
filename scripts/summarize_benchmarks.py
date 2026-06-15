#!/usr/bin/env python3

import json
import pathlib


INPUT = pathlib.Path("docs/benchmarks/benchmark_results.json")
OUTPUT = pathlib.Path("docs/benchmarks/benchmark_summary.md")


def ns_to_display(ns: float) -> str:
    if ns >= 1_000:
        return f"{ns / 1_000:.2f} µs"
    return f"{ns:.2f} ns"


def main() -> None:
    data = json.loads(INPUT.read_text())

    rows = {}

    for bench in data["benchmarks"]:
        run_name = bench["run_name"]
        aggregate = bench.get("aggregate_name")

        if aggregate not in {"mean", "median", "stddev", "cv"}:
            continue

        rows.setdefault(run_name, {})[aggregate] = bench

    lines = []
    lines.append("### Benchmarks")
    lines.append("")
    lines.append(
        "Benchmarks were run in Release mode with Google Benchmark. "
        "The fixture reconstruction benchmark includes JSON parsing from a recorded Polymarket CLOB order book. "
        "The synthetic full-pipeline benchmark excludes JSON parsing and uses pre-generated market-data events."
    )
    lines.append("")
    lines.append("| Benchmark | Mean | Median | Stddev | CV | Notes |")
    lines.append("|---|---:|---:|---:|---:|---|")

    notes = {
        "BM_PolymarketFixture_ReconstructBook":
            "Recorded Polymarket JSON fixture → MarketEvent queue → BookBuilder → OrderBook",
        "BM_TradingLoop_StaticOpportunityToPaperOrders":
            "Static reconstructed books → Strategy → IntentBuilder → RiskManager → PaperOMS",
        "BM_SyntheticUpdates_FullPipeline":
            "Pre-generated updates → BookBuilder → OrderBook → Strategy → RiskManager → PaperOMS",
    }

    for name, aggregates in rows.items():
        mean = aggregates.get("mean", {}).get("real_time")
        median = aggregates.get("median", {}).get("real_time")
        stddev = aggregates.get("stddev", {}).get("real_time")
        cv = aggregates.get("cv", {}).get("real_time")

        if mean is None or median is None:
            continue

        lines.append(
            f"| `{name}` | "
            f"{ns_to_display(mean)} | "
            f"{ns_to_display(median)} | "
            f"{ns_to_display(stddev or 0)} | "
            f"{(cv or 0) * 100:.2f}% | "
            f"{notes.get(name, '')} |"
        )

    lines.append("")
    lines.append("> Results are machine-dependent and should be interpreted as relative project measurements, not exchange-colocation latency claims.")
    lines.append("")

    OUTPUT.write_text("\n".join(lines))


if __name__ == "__main__":
    main()