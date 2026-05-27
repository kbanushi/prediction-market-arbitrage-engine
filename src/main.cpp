#include <iostream>
#include <vector>
#include <unordered_map>
#include "strategy.hpp"
#include "types.hpp"

int main(){
    Market btc90 {1, 5200, 5300, 4700, 4600, 700, 100};
    Market btc100 {2, 6500, 5900, 4200, 4100, 700, 100};

    Constraint constraint;
    std::unordered_map<uint32_t, Market> markets;
    std::vector<Constraint> constraints;

    markets[btc90.id] = btc90;
    markets[btc100.id] = btc100;

    constraint.strong_market_id = btc100.id;
    constraint.weak_market_id = btc90.id;

    constraints.push_back(constraint);

    std::vector<Opportunity> opportunities = evaluate(markets, constraints);

    std::cout << "here" << std::endl;
    
    for (const Opportunity& o : opportunities){
        std::cout << o.buy_market_id << " " << o.sell_market_id << " " << "net edge: " << o.net_edge << std::endl;
    }

}

