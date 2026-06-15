#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

#include "order_types.hpp"

constexpr uint32_t MAX_PAPER_ORDERS = 4096;

class PaperOMS{
    public: 
        PaperOMS() = default;

        SubmitResult submit_order(const OrderIntent& intent);
        CancelStatus cancel_order(uint64_t order_id, uint64_t timestamp_ns);
        FillStatus apply_fill(uint64_t order_id, uint32_t fill_quantiy, uint64_t timestamp_ns);
        
        const PaperOrder* get_order(uint64_t order_id);

        uint32_t order_count() const;
        uint64_t next_order_id() const;

        void reset();
    private:
        PaperOrder* find_order(uint64_t order_id);
        static bool is_terminal(OrderStatus status);

        std::array<PaperOrder, MAX_PAPER_ORDERS> orders{};
        uint32_t order_count_ = 0;
        uint64_t next_order_id_ = 1;
};