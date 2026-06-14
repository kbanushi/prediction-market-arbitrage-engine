#pragma once

#include <array>
#include <cstdint>

#include "order_types.hpp"

constexpr uint32_t MAX_RISK_MARKETS = 1024;
constexpr uint32_t MAX_PREDICTION_PRICE = 10000;

enum class RiskStatus : uint8_t{
    Approved, 
    InvalidMarket,
    MarketDisabled,
    InvalidQuantity,
    InvalidPrice,
    QuantityLimitExceeded,
    NotionalLimitExceeded
};

struct RiskLimits{
    uint64_t max_order_notional = 0;
    uint64_t max_order_quantity = 0;
};

struct RiskDecision{
    uint64_t computed_notional = 0;
    RiskStatus status = RiskStatus::Approved;
};

class RiskManager{
    public:
        explicit RiskManager(RiskLimits limits);

        bool enable_market(uint32_t internal_market_id);
        bool disable_market(uint32_t internal_market_id);
        bool is_market_enabled(uint32_t internal_market_id) const;

        RiskDecision check_order(const OrderIntent& intent) const;

    private:
        uint64_t compute_notional(const OrderIntent& intent) const;

    private:
        RiskLimits limits_{};
        std::array<bool, MAX_RISK_MARKETS> markets_enabled_{};
};