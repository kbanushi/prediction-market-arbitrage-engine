#include <catch2/catch_test_macros.hpp>

#include "../src/order_intent_builder.hpp"
#include "../src/order_types.hpp"
#include "../src/types.hpp"

namespace {

Opportunity make_opportunity() {
    Opportunity opportunity{};
    opportunity.buy_market_id = 9000;
    opportunity.sell_market_id = 10000;
    opportunity.buy_price = 5000;
    opportunity.sell_price = 6000;
    opportunity.gross_edge = 1000;
    opportunity.net_edge = 900;
    opportunity.estimated_fees = 100;
    opportunity.max_size = 40;
    return opportunity;
}

} // namespace

TEST_CASE("OrderIntentBuilder creates buy and sell intents from opportunity", "[order_intent_builder]") {
    OrderIntentBuilder builder;

    Opportunity opportunity = make_opportunity();

    IntentPair pair = builder.build_from_opportunity(opportunity, 123456);

    REQUIRE(pair.buy_order.intent_id == 1);
    REQUIRE(pair.buy_order.created_timestamp_ns == 123456);
    REQUIRE(pair.buy_order.internal_market_id == opportunity.buy_market_id);
    REQUIRE(pair.buy_order.price == opportunity.buy_price);
    REQUIRE(pair.buy_order.quantity == opportunity.max_size);
    REQUIRE(pair.buy_order.side == TradeSide::Buy);
    REQUIRE(pair.buy_order.time_in_force == TimeInForce::IOC);

    REQUIRE(pair.sell_order.intent_id == 2);
    REQUIRE(pair.sell_order.created_timestamp_ns == 123456);
    REQUIRE(pair.sell_order.internal_market_id == opportunity.sell_market_id);
    REQUIRE(pair.sell_order.price == opportunity.sell_price);
    REQUIRE(pair.sell_order.quantity == opportunity.max_size);
    REQUIRE(pair.sell_order.side == TradeSide::Sell);
    REQUIRE(pair.sell_order.time_in_force == TimeInForce::IOC);
}

TEST_CASE("OrderIntentBuilder assigns increasing intent ids across opportunities", "[order_intent_builder]") {
    OrderIntentBuilder builder;

    Opportunity first = make_opportunity();
    Opportunity second = make_opportunity();

    IntentPair first_pair = builder.build_from_opportunity(first, 1000);
    IntentPair second_pair = builder.build_from_opportunity(second, 2000);

    REQUIRE(first_pair.buy_order.intent_id == 1);
    REQUIRE(first_pair.sell_order.intent_id == 2);

    REQUIRE(second_pair.buy_order.intent_id == 3);
    REQUIRE(second_pair.sell_order.intent_id == 4);

    REQUIRE(second_pair.buy_order.created_timestamp_ns == 2000);
    REQUIRE(second_pair.sell_order.created_timestamp_ns == 2000);
}

TEST_CASE("OrderIntentBuilder preserves zero max size for downstream validation", "[order_intent_builder]") {
    OrderIntentBuilder builder;

    Opportunity opportunity = make_opportunity();
    opportunity.max_size = 0;

    IntentPair pair = builder.build_from_opportunity(opportunity, 1000);

    REQUIRE(pair.buy_order.quantity == 0);
    REQUIRE(pair.sell_order.quantity == 0);
}