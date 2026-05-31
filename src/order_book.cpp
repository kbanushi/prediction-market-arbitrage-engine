#include "order_book.hpp"

OrderBook::OrderBook(size_t max_active_orders) : free_head(0){
    //Pre-allocate vector size with max active orders, should be adjusted based on expected # of active orders for a market
    memory_pool.resize(max_active_orders);

    for (uint32_t i = 0; i < max_active_orders; i++){
        if (i == max_active_orders - 1){
            memory_pool[i].next_index = -1;
        }
        else{
            memory_pool[i].next_index = i + 1;
        }

        memory_pool[i].prev_index = -1;
    }

    id_to_index.reserve(max_active_orders); //pre allocate map
}

int32_t OrderBook::allocate_order(){
    if (free_head == -1){
        throw std::runtime_error("Fatal: Memory pool exhausted.  Increase max_active_orders");
    }

    int32_t allocated_index = free_head;

    free_head = memory_pool[allocated_index].next_index;

    memory_pool[allocated_index].next_index = -1;
    memory_pool[allocated_index].prev_index = -1;

    return allocated_index;
}

void OrderBook::free_order(int32_t target_index){
    memory_pool[target_index].next_index = free_head;
    memory_pool[target_index].prev_index = -1;

    free_head = target_index;
}

void OrderBook::insert_order(uint64_t internal_id, char side, uint32_t price, uint32_t quantity){
    if (price >= MAX_PRICE_POINTS){
        throw std::runtime_error("Fatal: Price greater than max price points");
    }

    int32_t allocated_index = allocate_order();

    Order& order = memory_pool[allocated_index];
    order.internal_id = internal_id;
    order.price = price;
    order.quantity = quantity;

    id_to_index[internal_id] = allocated_index;

    PriceLevel& level = (side == 'B') ? bid_levels[price] : ask_levels[price];
    level.total_volume += quantity;

    if (level.head_index == -1){ //First order on this price level
        level.head_index = allocated_index;
        level.tail_index = allocated_index;
    }
    else{ // append to tail
        int32_t old_tail_index = level.tail_index;

        memory_pool[old_tail_index].next_index = allocated_index;
        order.prev_index = old_tail_index;
        level.tail_index = allocated_index;
    }

    if (side == 'B'){
        if (best_bid_price == -1 || price > static_cast<uint32_t>(best_bid_price)){
            best_bid_price = static_cast<int32_t>(price);
        }
    }
    else{
        if (best_ask_price == -1 || price < static_cast<uint32_t>(best_ask_price)){
            best_ask_price = static_cast<int32_t>(price);
        }
    }
}

void OrderBook::remove_from_list(int32_t index, PriceLevel& level){
    Order& target_order = memory_pool[index];

    if (target_order.prev_index != -1){
        memory_pool[target_order.prev_index].next_index = target_order.next_index;
    }else{
        level.head_index = target_order.next_index;
    }

    if (target_order.next_index != -1){
        memory_pool[target_order.next_index].prev_index = target_order.prev_index;
    }
    else{
        level.tail_index = target_order.prev_index;
    }
}

void OrderBook::cancel_order(uint64_t internal_id, char side, uint32_t price){
    auto it = id_to_index.find(internal_id);
    if (it == id_to_index.end()) return; //Order doesn't exist. Might've been filled in market.

    int32_t target_index = it->second;
    Order& target_order = memory_pool[target_index];

    PriceLevel& level = (side == 'B') ? bid_levels[price] : ask_levels[price];
    level.total_volume -= target_order.quantity;

    remove_from_list(target_index, level);

    id_to_index.erase(it);
    free_order(target_index);

    if (level.head_index == -1){
        if (side == 'B' && static_cast<int32_t>(price) == best_bid_price){
            best_bid_price = -1;
            for (int32_t p = static_cast<int32_t>(price) - 1; p >= 0; p--){
                if (bid_levels[p].head_index != -1){
                    best_bid_price = p;
                    return;
                }
            }
        }
        else if (side != 'B' && static_cast<int32_t>(price) == best_ask_price){
            best_ask_price = -1;
            for (int32_t p = static_cast<int32_t>(price) + 1; p < static_cast<int32_t>(MAX_PRICE_POINTS); p++){
                if (ask_levels[p].head_index != -1){
                    best_ask_price = p;
                    return;
                }
            }
        }
    }
}

int32_t OrderBook::get_best_bid() const {
    return best_bid_price;
}

int32_t OrderBook::get_best_ask() const {
    return best_ask_price;
}

uint32_t OrderBook::get_volume_at_price(char side, uint32_t price) const {
    return (side == 'B') ? bid_levels[price].total_volume : ask_levels[price].total_volume;
}
