#include "risk_manager.hpp"

RiskManager::RiskManager(RiskLimits limits) : limits_(limits) {}

bool RiskManager::enable_market(uint32_t internal_market_id){
    if (internal_market_id >= MAX_RISK_MARKETS) [[unlikely]] {
        return false;
    }

    markets_enabled_[internal_market_id] = true;
    return true;
}

bool RiskManager::disable_market(uint32_t internal_market_id){
    if (internal_market_id >= MAX_RISK_MARKETS) [[unlikely]]{
        return false;
    }

    markets_enabled_[internal_market_id] = false;
    return true;
} 

bool RiskManager::is_market_enabled(uint32_t internal_market_id) const {
    if (internal_market_id >= MAX_RISK_MARKETS) [[unlikely]] {
        return false;
    }

    return markets_enabled_[internal_market_id];
}

RiskDecision RiskManager::check_order(const OrderIntent& intent) const {
    if (intent.internal_market_id >= MAX_RISK_MARKETS) [[unlikely]] {
        return RiskDecision{0, RiskStatus::InvalidMarket};
    }

    if (!is_market_enabled(intent.internal_market_id)) [[unlikely]] {
        return RiskDecision{0, RiskStatus::MarketDisabled};
    }

    if (intent.quantity == 0) [[unlikely]] {
        return RiskDecision{0, RiskStatus::InvalidQuantity};
    }

    if (intent.price == 0 || intent.price > MAX_PREDICTION_PRICE) [[unlikely]] {
        return RiskDecision{0, RiskStatus::InvalidPrice};
    }

    if (limits_.max_order_quantity > 0 && intent.quantity > limits_.max_order_quantity) [[unlikely]] {
        return RiskDecision{0, RiskStatus::QuantityLimitExceeded};
    }

    const uint64_t notional = compute_notional(intent);

    if (limits_.max_order_notional > 0 && notional > limits_.max_order_notional) [[unlikely]] {
        return RiskDecision{notional, RiskStatus::NotionalLimitExceeded};
    }

    return RiskDecision{notional, RiskStatus::Approved};
}

uint64_t RiskManager::compute_notional(const OrderIntent& intent) const {
    return (static_cast<uint64_t>(intent.price) * static_cast<uint64_t>(intent.quantity)) / MAX_PREDICTION_PRICE;
}

