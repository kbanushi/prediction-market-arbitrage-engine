#include <benchmark/benchmark.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../src/book_builder.hpp"
#include "../src/order_book.hpp"
#include "../src/order_intent_builder.hpp"
#include "../src/paper_oms.hpp"
#include "../src/polymarket_adapter.hpp"
#include "../src/risk_manager.hpp"
#include "../src/spsc_queue.hpp"
#include "../src/strategy.hpp"

namespace {

std::string read_file_or_die(const std::string& path) {
    std::ifstream file(path);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open benchmark fixture: " + path);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<MarketEvent> make_synthetic_update_stream(std::size_t event_count) {
    std::vector<MarketEvent> events;
    events.reserve(event_count);

    uint64_t weak_sequence = 1;
    uint64_t strong_sequence = 1;

    for (std::size_t i = 0; i < event_count; ++i) {
        MarketEvent event{};

        if ((i & 1U) == 0) {
            event.sequence_number = weak_sequence++;
            event.exchange_timestamp_ns = static_cast<uint64_t>(i);
            event.receive_timestamp_ns = static_cast<uint64_t>(i);
            event.market_id = 0;
            event.price = 5000;
            event.quantity = 40 + static_cast<uint32_t>(i % 16);
            event.type = EventType::LevelSet;
            event.side = Side::Ask;
        } else {
            event.sequence_number = strong_sequence++;
            event.exchange_timestamp_ns = static_cast<uint64_t>(i);
            event.receive_timestamp_ns = static_cast<uint64_t>(i);
            event.market_id = 1;
            event.price = 6000;
            event.quantity = 40 + static_cast<uint32_t>(i % 16);
            event.type = EventType::LevelSet;
            event.side = Side::Bid;
        }

        events.push_back(event);
    }

    return events;
}

} // namespace

static void BM_PolymarketFixture_ReconstructBook(benchmark::State& state) {
    const std::string payload = read_file_or_die(
        "tests/fixtures/polymarket/book_fed_yes.json"
    );

    for (auto _ : state) {
        SPSCQueue<MarketEvent, 1024> queue;
        OrderBook book;
        BookBuilder builder(book);

        auto parse_result = polymarket::parse_book_snapshot(
            payload,
            0,
            1,
            queue
        );

        auto drain_result = builder.drain(queue);

        benchmark::DoNotOptimize(parse_result.events_emitted);
        benchmark::DoNotOptimize(drain_result.events_consumed);
        benchmark::DoNotOptimize(drain_result.events_applied);
        benchmark::DoNotOptimize(book.get_best_bid());
        benchmark::DoNotOptimize(book.get_best_ask());
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_PolymarketFixture_ReconstructBook);

static void BM_TradingLoop_StaticOpportunityToPaperOrders(benchmark::State& state) {
    OrderBook weak_book;
    OrderBook strong_book;

    weak_book.set_level(Side::Ask, 5000, 100);
    strong_book.set_level(Side::Bid, 6000, 40);

    Strategy strategy;
    strategy.configure_market(0, 0, &weak_book);
    strategy.configure_market(1, 0, &strong_book);
    strategy.configure_constraint(1, 0);

    RiskLimits limits{};
    limits.max_order_quantity = 100;
    limits.max_order_notional = 1000000;

    RiskManager risk(limits);
    risk.enable_market(0);
    risk.enable_market(1);

    OrderIntentBuilder intent_builder;

    std::array<Opportunity, 4> opportunities{};

    for (auto _ : state) {
        PaperOMS oms;

        uint32_t opportunities_found = strategy.evaluate_channels(
            opportunities.data(),
            static_cast<uint32_t>(opportunities.size())
        );

        if (opportunities_found == 0) [[unlikely]] {
            benchmark::DoNotOptimize(opportunities_found);
            continue;
        }

        IntentPair intents = intent_builder.build_from_opportunity(
            opportunities[0],
            123456
        );

        RiskDecision buy_risk = risk.check_order(intents.buy_order);
        RiskDecision sell_risk = risk.check_order(intents.sell_order);

        if (buy_risk.status == RiskStatus::Approved) {
            SubmitResult buy_submit = oms.submit_order(intents.buy_order);
            benchmark::DoNotOptimize(buy_submit);
        }

        if (sell_risk.status == RiskStatus::Approved) {
            SubmitResult sell_submit = oms.submit_order(intents.sell_order);
            benchmark::DoNotOptimize(sell_submit);
        }

        benchmark::DoNotOptimize(opportunities_found);
        benchmark::DoNotOptimize(buy_risk);
        benchmark::DoNotOptimize(sell_risk);
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_TradingLoop_StaticOpportunityToPaperOrders);

static void BM_SyntheticUpdates_FullPipeline(benchmark::State& state) {
    constexpr std::size_t EVENT_COUNT = 8192;

    const std::vector<MarketEvent> events =
        make_synthetic_update_stream(EVENT_COUNT);

    for (auto _ : state) {
        state.PauseTiming();

        OrderBook weak_book;
        OrderBook strong_book;

        BookBuilder weak_builder(weak_book);
        BookBuilder strong_builder(strong_book);

        Strategy strategy;
        strategy.configure_market(0, 0, &weak_book);
        strategy.configure_market(1, 0, &strong_book);
        strategy.configure_constraint(1, 0);

        RiskLimits limits{};
        limits.max_order_quantity = 1000000;
        limits.max_order_notional = 1000000;

        RiskManager risk(limits);
        risk.enable_market(0);
        risk.enable_market(1);

        OrderIntentBuilder intent_builder;
        PaperOMS oms;

        std::array<Opportunity, 4> opportunities{};

        uint64_t orders_submitted = 0;

        state.ResumeTiming();

        for (const MarketEvent& event : events) {
            BookBuildResult build_result{};

            if (event.market_id == 0) {
                build_result = weak_builder.process_event(event);
            } else {
                build_result = strong_builder.process_event(event);
            }

            if (build_result.status != BookBuildStatus::Applied) [[unlikely]] {
                benchmark::DoNotOptimize(build_result);
                continue;
            }

            uint32_t opportunity_count = strategy.evaluate_channels(
                opportunities.data(),
                static_cast<uint32_t>(opportunities.size())
            );

            if (opportunity_count == 0) {
                benchmark::DoNotOptimize(opportunity_count);
                continue;
            }

            IntentPair intents = intent_builder.build_from_opportunity(
                opportunities[0],
                event.receive_timestamp_ns
            );

            RiskDecision buy_risk = risk.check_order(intents.buy_order);
            RiskDecision sell_risk = risk.check_order(intents.sell_order);

            if (buy_risk.status == RiskStatus::Approved) {
                SubmitResult buy_submit = oms.submit_order(intents.buy_order);
                benchmark::DoNotOptimize(buy_submit);
                ++orders_submitted;
            }

            if (sell_risk.status == RiskStatus::Approved) {
                SubmitResult sell_submit = oms.submit_order(intents.sell_order);
                benchmark::DoNotOptimize(sell_submit);
                ++orders_submitted;
            }

            if (oms.order_count() > 900) [[unlikely]] {
                oms.reset();
            }

            benchmark::DoNotOptimize(opportunity_count);
            benchmark::DoNotOptimize(buy_risk);
            benchmark::DoNotOptimize(sell_risk);
        }

        benchmark::DoNotOptimize(orders_submitted);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(EVENT_COUNT)
    );
}

BENCHMARK(BM_SyntheticUpdates_FullPipeline)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();