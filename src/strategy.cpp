#include "strategy.hpp"

std::vector<Opportunity> evaluate(const std::unordered_map<uint32_t, Market>& markets, const std::vector<Constraint>& constraints){
    std::vector<Opportunity> opportunities;
    
    for (const Constraint& c : constraints){
        const Market& strong_market = markets.at(c.strong_market_id);
        const Market& weak_market = markets.at(c.weak_market_id);

        int32_t fees = (strong_market.yes_bid * strong_market.fee_rate) / 10000 + (weak_market.yes_ask * weak_market.fee_rate) / 10000;
        int32_t gross_edge = strong_market.yes_bid - weak_market.yes_ask;
        int32_t net_edge = gross_edge - fees;

        if (net_edge > 0){
            Opportunity o;

            o.buy_market_id = weak_market.id;
            o.sell_market_id = strong_market.id;
            o.buy_price = weak_market.yes_ask;
            o.sell_price = strong_market.yes_bid;
            o.gross_edge = gross_edge;
            o.net_edge = net_edge;
            o.max_size = std::min(strong_market.available_size, weak_market.available_size);
            o.estimated_fees = fees;


            opportunities.push_back(o);
        }
    }

    return opportunities;
}