#include "order_intent_builder.hpp"

IntentPair OrderIntentBuilder::build_from_opportunity(const Opportunity& opportunity, uint64_t timestamp_ns){
    IntentPair pair{};
    
    pair.buy_order.intent_id = next_intent_id++;
    pair.buy_order.created_timestamp_ns = timestamp_ns;
    pair.buy_order.internal_market_id = opportunity.buy_market_id;
    pair.buy_order.price = opportunity.buy_price;
    pair.buy_order.quantity = opportunity.max_size;
    pair.buy_order.side = TradeSide::Buy;
    pair.buy_order.time_in_force = TimeInForce::IOC;

    pair.sell_order.intent_id = next_intent_id++;
    pair.sell_order.created_timestamp_ns = timestamp_ns;
    pair.sell_order.internal_market_id = opportunity.sell_market_id;
    pair.sell_order.price = opportunity.sell_price;
    pair.sell_order.quantity = opportunity.max_size;
    pair.sell_order.side = TradeSide::Sell;
    pair.sell_order.time_in_force = TimeInForce::IOC;

    return pair;
}

