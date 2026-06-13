#include "paper_oms.hpp"

SubmitResult PaperOMS::submit_order(const OrderIntent& intent){
    if (intent.quantity == 0) [[unlikely]] {
        return SubmitResult{0, SubmitStatus::InvalidQuantity};
    }

    if (order_count_ >= MAX_PAPER_ORDERS) [[unlikely]] {
        return SubmitResult{0, SubmitStatus::CapacityExceeded};
    }

    const uint64_t order_id = next_order_id_++;

    PaperOrder& order = orders[order_count_++];
    order.order_id = order_id;
    order.intent_id = intent.intent_id;
    order.created_timestamp_ns = intent.created_timestamp_ns;
    order.updated_timestamp_ns = intent.created_timestamp_ns;

    order.internal_market_id = intent.internal_market_id;
    order.price = intent.price;
    order.original_quantity = intent.quantity;
    order.remaining_quantity = intent.quantity;
    order.filled_quantity = 0;

    order.side = intent.side;
    order.time_in_force = intent.time_in_force;
    order.status = OrderStatus::Accepted;

    return SubmitResult{order_id, SubmitStatus::Accepted};
}

CancelStatus PaperOMS::cancel_order(uint64_t order_id, uint64_t timestamp_ns){
    PaperOrder* order = find_order(order_id);

    if (order == nullptr) [[unlikely]] {
        return CancelStatus::NotFound;
    }

    if (is_terminal(order->status)) [[unlikely]] {
        return CancelStatus::AlreadyTerminal;
    }

    order->status = OrderStatus::Cancelled;
    order->updated_timestamp_ns = timestamp_ns;

    return CancelStatus::Cancelled;
}

FillStatus PaperOMS::apply_fill(uint64_t order_id, uint32_t fill_quantity, uint64_t timestamp_ns){
    if (fill_quantity == 0) [[unlikely]] {
        return FillStatus::InvalidFillQuantity;
    }

    PaperOrder* order = find_order(order_id);

    if (order == nullptr) [[unlikely]] {
        return FillStatus::NotFound;
    }

    if (is_terminal(order->status)) [[unlikely]] {
        return FillStatus::AlreadyTerminal;
    }

    if (fill_quantity > order->remaining_quantity) [[unlikely]] {
        return FillStatus::FillQuantityTooLarge;
    }

    order->remaining_quantity -= fill_quantity;
    order->filled_quantity += fill_quantity;
    order->updated_timestamp_ns = timestamp_ns;

    if (order->remaining_quantity == 0){
        order->status = OrderStatus::Filled;
        return FillStatus::Filled;
    }

    order->status = OrderStatus::PartiallyFilled;
    return FillStatus::PartiallyFilled;
}

const PaperOrder* PaperOMS::get_order(uint64_t order_id) {
    return find_order(order_id);
}

uint32_t PaperOMS::order_count() const {
    return order_count_;
}

uint64_t PaperOMS::next_order_id() const {
    return next_order_id_;
}

// TODO: Refactor with flatmap and lookup after perf testing
PaperOrder* PaperOMS::find_order(uint64_t order_id) {
    for (size_t i = 0; i < order_count_; i++){
        if (orders[i].order_id == order_id){
            return &orders[i];
        }
    }
    return nullptr;
}

bool PaperOMS::is_terminal(OrderStatus status){
    return status == OrderStatus::Filled || status == OrderStatus::Cancelled || status == OrderStatus::Rejected;
}