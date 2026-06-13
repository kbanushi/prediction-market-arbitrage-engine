#pragma once

#include <cstdint>

#include "order_types.hpp"
#include "types.hpp"

struct IntentPair{
    OrderIntent buy_order{};
    OrderIntent sell_order{};
};

class OrderIntentBuilder{
    public:
        OrderIntentBuilder() = default;
        IntentPair build_from_opportunity(const Opportunity& opportunity, uint64_t timestamp_ns);

    private:
        uint64_t next_intent_id = 1;
};