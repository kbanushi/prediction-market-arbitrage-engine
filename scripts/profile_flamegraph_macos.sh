#!/usr/bin/env bash
set -euo pipefail

mkdir -p docs/flamegraphs tools

if [ ! -d tools/FlameGraph ]; then
  git clone https://github.com/brendangregg/FlameGraph tools/FlameGraph
fi

cmake -S . -B build-profile \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O3 -g -fno-omit-frame-pointer -DNDEBUG"

cmake --build build-profile -j

./build-profile/engine_benchmarks \
  --benchmark_filter=SyntheticUpdates \
  --benchmark_min_time=60s &

PID=$!

sleep 3

if ! kill -0 "$PID" 2>/dev/null; then
  echo "Benchmark process exited before sampling started."
  exit 1
fi

SAMPLE_OUT="docs/flamegraphs/synthetic_full_pipeline.sample.txt"
FOLDED_OUT="docs/flamegraphs/synthetic_full_pipeline.folded"
SVG_OUT="docs/flamegraphs/synthetic_full_pipeline.svg"

echo "Sampling PID $PID..."

if sample "$PID" 30 1 -file "$SAMPLE_OUT" -fullPaths; then
  echo "Sampled without sudo."
else
  echo "Sampling without sudo failed. Retrying with sudo..."
  sudo sample "$PID" 30 1 -file "$SAMPLE_OUT" -fullPaths
fi

echo "Stopping benchmark process..."
kill "$PID" 2>/dev/null || true
wait "$PID" 2>/dev/null || true

tools/FlameGraph/stackcollapse-sample.awk \
  "$SAMPLE_OUT" \
  > "$FOLDED_OUT"

tools/FlameGraph/flamegraph.pl \
  --title="Synthetic full pipeline" \
  --subtitle="MarketEvent → BookBuilder → OrderBook → Strategy → RiskManager → PaperOMS" \
  "$FOLDED_OUT" \
  > "$SVG_OUT"

echo "Wrote $SVG_OUT"