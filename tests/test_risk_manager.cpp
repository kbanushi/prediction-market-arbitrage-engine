#include <catch2/catch_test_macros.hpp>

#include "../src/risk_manager.hpp"
#include "../src/order_types.hpp"

namespace {

OrderIntent make_intent(
    uint32_t internal_market_id = 0,
    uint32_t price = 5200,
    uint32_t quantity = 1000
) {
    OrderIntent intent{};
    intent.intent_id = 1;
    intent.created_timestamp_ns = 1000;
    intent.internal_market_id = internal_market_id;
    intent.price = price;
    intent.quantity = quantity;
    intent.side = TradeSide::Buy;
    intent.time_in_force = TimeInForce::IOC;
    return intent;
}

RiskManager make_risk_manager() {
    RiskLimits limits{};
    limits.max_order_notional = 1000;
    limits.max_order_quantity = 2000;

    RiskManager risk(limits);
    risk.enable_market(0);
    risk.enable_market(1);

    return risk;
}

} // namespace

TEST_CASE("RiskManager approves valid order", "[risk_manager]") {
    RiskManager risk = make_risk_manager();

    RiskDecision decision = risk.check_order(make_intent());

    REQUIRE(decision.status == RiskStatus::Approved);
    REQUIRE(decision.computed_notional == 520);
}

TEST_CASE("RiskManager rejects order for invalid market id", "[risk_manager]") {
    RiskManager risk = make_risk_manager();

    OrderIntent intent = make_intent(MAX_RISK_MARKETS);

    RiskDecision decision = risk.check_order(intent);

    REQUIRE(decision.status == RiskStatus::InvalidMarket);
    REQUIRE(decision.computed_notional == 0);
}

TEST_CASE("RiskManager rejects disabled market", "[risk_manager]") {
    RiskManager risk = make_risk_manager();

    OrderIntent intent = make_intent(2);

    RiskDecision decision = risk.check_order(intent);

    REQUIRE(decision.status == RiskStatus::MarketDisabled);
    REQUIRE(decision.computed_notional == 0);
}

TEST_CASE("RiskManager enables and disables markets", "[risk_manager]") {
    RiskManager risk = make_risk_manager();

    REQUIRE(risk.is_market_enabled(0));
    REQUIRE(risk.disable_market(0));
    REQUIRE_FALSE(risk.is_market_enabled(0));

    OrderIntent intent = make_intent(0);
    RiskDecision decision = risk.check_order(intent);

    REQUIRE(decision.status == RiskStatus::MarketDisabled);

    REQUIRE(risk.enable_market(0));
    REQUIRE(risk.is_market_enabled(0));

    decision = risk.check_order(intent);

    REQUIRE(decision.status == RiskStatus::Approved);
}

TEST_CASE("RiskManager rejects zero quantity", "[risk_manager]") {
    RiskManager risk = make_risk_manager();

    OrderIntent intent = make_intent();
    intent.quantity = 0;

    RiskDecision decision = risk.check_order(intent);

    REQUIRE(decision.status == RiskStatus::InvalidQuantity);
}

TEST_CASE("RiskManager rejects zero price", "[risk_manager]") {
    RiskManager risk = make_risk_manager();

    OrderIntent intent = make_intent();
    intent.price = 0;

    RiskDecision decision = risk.check_order(intent);

    REQUIRE(decision.status == RiskStatus::InvalidPrice);
}

TEST_CASE("RiskManager rejects price above prediction max", "[risk_manager]") {
    RiskManager risk = make_risk_manager();

    OrderIntent intent = make_intent();
    intent.price = MAX_PREDICTION_PRICE + 1;

    RiskDecision decision = risk.check_order(intent);

    REQUIRE(decision.status == RiskStatus::InvalidPrice);
}

TEST_CASE("RiskManager rejects quantity above limit", "[risk_manager]") {
    RiskManager risk = make_risk_manager();

    OrderIntent intent = make_intent();
    intent.quantity = 2001;

    RiskDecision decision = risk.check_order(intent);

    REQUIRE(decision.status == RiskStatus::QuantityLimitExceeded);
}

TEST_CASE("RiskManager rejects notional above limit", "[risk_manager]") {
    RiskManager risk = make_risk_manager();

    OrderIntent intent = make_intent();
    intent.price = 9000;
    intent.quantity = 2000;

    RiskDecision decision = risk.check_order(intent);

    REQUIRE(decision.status == RiskStatus::NotionalLimitExceeded);
    REQUIRE(decision.computed_notional == 1800);
}

TEST_CASE("RiskManager allows unlimited quantity when quantity limit is zero", "[risk_manager]") {
    RiskLimits limits{};
    limits.max_order_quantity = 0;
    limits.max_order_notional = 1000000;

    RiskManager risk(limits);
    REQUIRE(risk.enable_market(0));

    OrderIntent intent = make_intent();
    intent.quantity = 100000;

    RiskDecision decision = risk.check_order(intent);

    REQUIRE(decision.status == RiskStatus::Approved);
}

TEST_CASE("RiskManager allows unlimited notional when notional limit is zero", "[risk_manager]") {
    RiskLimits limits{};
    limits.max_order_quantity = 1000000;
    limits.max_order_notional = 0;

    RiskManager risk(limits);
    REQUIRE(risk.enable_market(0));

    OrderIntent intent = make_intent();
    intent.quantity = 100000;

    RiskDecision decision = risk.check_order(intent);

    REQUIRE(decision.status == RiskStatus::Approved);
}

TEST_CASE("RiskManager rejects enabling out-of-range market", "[risk_manager]") {
    RiskManager risk = make_risk_manager();

    REQUIRE_FALSE(risk.enable_market(MAX_RISK_MARKETS));
    REQUIRE_FALSE(risk.disable_market(MAX_RISK_MARKETS));
    REQUIRE_FALSE(risk.is_market_enabled(MAX_RISK_MARKETS));
}