#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>

#include "../src/book_builder.hpp"
#include "../src/order_book.hpp"
#include "../src/order_intent_builder.hpp"
#include "../src/paper_oms.hpp"
#include "../src/polymarket_adapter.hpp"
#include "../src/risk_manager.hpp"
#include "../src/spsc_queue.hpp"
#include "../src/strategy.hpp"

namespace {

void load_snapshot_into_book(
    const std::string& payload,
    uint32_t internal_market_id,
    OrderBook& book
) {
    SPSCQueue<MarketEvent, 1024> queue;
    BookBuilder builder(book);

    const auto parse_result = polymarket::parse_book_snapshot(
        payload,
        internal_market_id,
        1,
        queue
    );

    REQUIRE(parse_result.status == polymarket::SnapshotParseStatus::Ok);

    const auto drain_result = builder.drain(queue);

    REQUIRE(drain_result.status == BookDrainStatus::Drained);
    REQUIRE(drain_result.events_consumed == parse_result.events_emitted);
    REQUIRE(drain_result.events_applied == parse_result.events_emitted);
    REQUIRE(builder.last_sequence_number() == parse_result.events_emitted);
}

} // namespace

TEST_CASE("End-to-end pipeline converts Polymarket snapshots into risk-approved paper orders", "[integration][end_to_end]") {
    const std::string weak_market_snapshot = R"json(
    {
        "market": "0xweak",
        "asset_id": "weak-token",
        "timestamp": "1234567890",
        "hash": "weak-hash",
        "bids": [
            { "price": "0.48", "size": "100" }
        ],
        "asks": [
            { "price": "0.50", "size": "40" }
        ],
        "min_order_size": "1",
        "tick_size": "0.01",
        "neg_risk": false,
        "last_trade_price": "0.49"
    }
    )json";

    const std::string strong_market_snapshot = R"json(
    {
        "market": "0xstrong",
        "asset_id": "strong-token",
        "timestamp": "1234567890",
        "hash": "strong-hash",
        "bids": [
            { "price": "0.60", "size": "40" }
        ],
        "asks": [
            { "price": "0.62", "size": "100" }
        ],
        "min_order_size": "1",
        "tick_size": "0.01",
        "neg_risk": false,
        "last_trade_price": "0.61"
    }
    )json";

    OrderBook weak_book;
    OrderBook strong_book;

    load_snapshot_into_book(
        weak_market_snapshot,
        0,
        weak_book
    );

    load_snapshot_into_book(
        strong_market_snapshot,
        1,
        strong_book
    );

    REQUIRE(weak_book.get_best_ask() == 5000);
    REQUIRE(strong_book.get_best_bid() == 6000);

    Strategy strategy;
    strategy.configure_market(0, 0, &weak_book);
    strategy.configure_market(1, 0, &strong_book);
    strategy.configure_constraint(1, 0);

    std::array<Opportunity, 4> opportunities{};

    const uint32_t opportunities_found = strategy.evaluate_channels(
        opportunities.data(),
        static_cast<uint32_t>(opportunities.size())
    );

    REQUIRE(opportunities_found == 1);

    const Opportunity& opportunity = opportunities[0];

    REQUIRE(opportunity.buy_market_id == 0);
    REQUIRE(opportunity.sell_market_id == 1);
    REQUIRE(opportunity.buy_price == 5000);
    REQUIRE(opportunity.sell_price == 6000);
    REQUIRE(opportunity.gross_edge == 1000);
    REQUIRE(opportunity.net_edge == 1000);

    // Adapter scales size by SIZE_SCALE.
    REQUIRE(opportunity.max_size == 40 * polymarket::SIZE_SCALE);

    OrderIntentBuilder intent_builder;

    IntentPair intents = intent_builder.build_from_opportunity(
        opportunity,
        123456
    );

    REQUIRE(intents.buy_order.internal_market_id == 0);
    REQUIRE(intents.buy_order.side == TradeSide::Buy);
    REQUIRE(intents.buy_order.price == 5000);
    REQUIRE(intents.buy_order.quantity == 40 * polymarket::SIZE_SCALE);

    REQUIRE(intents.sell_order.internal_market_id == 1);
    REQUIRE(intents.sell_order.side == TradeSide::Sell);
    REQUIRE(intents.sell_order.price == 6000);
    REQUIRE(intents.sell_order.quantity == 40 * polymarket::SIZE_SCALE);

    RiskLimits limits{};
    limits.max_order_quantity = 100 * polymarket::SIZE_SCALE;
    limits.max_order_notional = 100 * polymarket::SIZE_SCALE;

    RiskManager risk(limits);
    REQUIRE(risk.enable_market(0));
    REQUIRE(risk.enable_market(1));

    RiskDecision buy_risk = risk.check_order(intents.buy_order);
    RiskDecision sell_risk = risk.check_order(intents.sell_order);

    REQUIRE(buy_risk.status == RiskStatus::Approved);
    REQUIRE(sell_risk.status == RiskStatus::Approved);

    PaperOMS oms;

    SubmitResult buy_submit = oms.submit_order(intents.buy_order);
    SubmitResult sell_submit = oms.submit_order(intents.sell_order);

    REQUIRE(buy_submit.status == SubmitStatus::Accepted);
    REQUIRE(sell_submit.status == SubmitStatus::Accepted);

    REQUIRE(buy_submit.order_id == 1);
    REQUIRE(sell_submit.order_id == 2);
    REQUIRE(oms.order_count() == 2);

    const PaperOrder* buy_order = oms.get_order(buy_submit.order_id);
    const PaperOrder* sell_order = oms.get_order(sell_submit.order_id);

    REQUIRE(buy_order != nullptr);
    REQUIRE(sell_order != nullptr);

    REQUIRE(buy_order->status == OrderStatus::Accepted);
    REQUIRE(sell_order->status == OrderStatus::Accepted);

    REQUIRE(buy_order->internal_market_id == 0);
    REQUIRE(sell_order->internal_market_id == 1);

    REQUIRE(buy_order->side == TradeSide::Buy);
    REQUIRE(sell_order->side == TradeSide::Sell);

    REQUIRE(buy_order->price == 5000);
    REQUIRE(sell_order->price == 6000);

    REQUIRE(buy_order->original_quantity == 40 * polymarket::SIZE_SCALE);
    REQUIRE(sell_order->original_quantity == 40 * polymarket::SIZE_SCALE);
}