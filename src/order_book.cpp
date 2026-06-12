#include "order_book.hpp"

void OrderBook::set_level(Side side, uint32_t price, uint32_t quantity){
    if (price >= MAX_PRICE_POINTS) [[unlikely]] {
        throw std::runtime_error("Fatal: Price greater than max price points");
    }

    if (side == Side::Bid){
        bid_levels[price] = quantity;
    }
    else{
        ask_levels[price] = quantity;
    }

    if (side == Side::Bid){
        if (quantity > 0){
            if (best_bid_price == -1 || price > static_cast<uint32_t>(best_bid_price)){
                best_bid_price = static_cast<int32_t>(price);
            }
        }
        else if (best_bid_price == static_cast<int32_t>(price)){
            recompute_best_bid();
        }
        
    }
    else{
        if (quantity > 0) {
            if (best_ask_price == -1 || price < static_cast<uint32_t>(best_ask_price)) {
                best_ask_price = static_cast<int32_t>(price);
            }
        } else if (best_ask_price == static_cast<int32_t>(price)) {
            recompute_best_ask();
        }
    }
}

void OrderBook::recompute_best_bid(){
    best_bid_price = -1;

    for (int32_t price = static_cast<int32_t>(MAX_PRICE_POINTS) - 1; price >= 0; --price) {
        if (bid_levels[price] > 0) {
            best_bid_price = price;
            return;
        }
    }
}

void OrderBook::recompute_best_ask(){
    best_ask_price = -1;

    for (uint32_t price = 0; price < MAX_PRICE_POINTS; ++price) {
        if (ask_levels[price] > 0) {
            best_ask_price = static_cast<int32_t>(price);
            return;
        }
    }
}

void OrderBook::clear_level(Side side, uint32_t price){
    set_level(side, price, 0);
}

bool OrderBook::apply(const MarketEvent& event){
    switch (event.type){
        case EventType::SnapshotBegin:
            clear();
            return true;

        case EventType::SnapshotLevel:
            set_level(event.side, event.price, event.quantity);
            return true;

        case EventType::SnapshotEnd:
            return true;

        case EventType::LevelSet:
            set_level(event.side, event.price, event.quantity);
            return true;
        
        case EventType::LevelClear:
            clear_level(event.side, event.price);
            return true;

        default:
            return false;
    }
}

int32_t OrderBook::get_best_bid() const {
    return best_bid_price;
}

int32_t OrderBook::get_best_ask() const {
    return best_ask_price;
}

uint32_t OrderBook::get_volume_at_price(Side side, uint32_t price) const {
    if (side == Side::Bid){
        return bid_levels[price];
    }
    
    return ask_levels[price];
}

void OrderBook::clear(){
    for (size_t i = MAX_PRICE_POINTS - 1; i >= 0; i--){
        bid_levels[i] = 0;
        ask_levels[i] = 0;
    }

    best_ask_price = -1;
    best_ask_price = -1;
}
