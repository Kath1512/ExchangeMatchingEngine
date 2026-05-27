#include <iostream>
#include "order_book.h"
#include "trade.h"
#include <map>

bool OrderBook::is_empty() const {
    return (bids_.empty() && asks_.empty());
}

bool OrderBook::has_bids() const {
    return !bids_.empty();
}

bool OrderBook::has_asks() const {
    return !asks_.empty();
}

Price OrderBook::best_bid() const {
    if(!has_bids()){
        throw std::runtime_error("Bids are empty");
    }

    return bids_.begin()->second.get_price();
}

Price OrderBook::best_ask() const {
    if(!has_asks()){
        throw std::runtime_error("Asks are empty");
    }

    return asks_.begin()->second.get_price();
}

const TradeHistory& OrderBook::get_trades() const {
    return trades_;
}

bool OrderBook::has_level(Side side, Price price) const {
    if (side == Side::Buy) return bids_.count(price) > 0;
    return asks_.count(price) > 0;
}

Quantity OrderBook::level_quantity(Side side, Price price) const {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        return it != bids_.end() ? it->second.get_total_quantity() : 0;
    }
    auto it = asks_.find(price);
    return it != asks_.end() ? it->second.get_total_quantity() : 0;
}

std::size_t OrderBook::level_order_count(Side side, Price price) const {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        return it != bids_.end() ? it->second.get_order_count() : 0;
    }
    auto it = asks_.find(price);
    return it != asks_.end() ? it->second.get_order_count() : 0;
}

std::size_t OrderBook::bid_level_count() const { return bids_.size(); }
std::size_t OrderBook::ask_level_count() const { return asks_.size(); }

bool OrderBook::can_fully_fill(const Order& order) const {
    if(order.is_buy()){
        return can_fully_fill_against_side(asks_, order, cmp_buy);
    }
    return can_fully_fill_against_side(bids_, order, cmp_sell);
}

bool OrderBook::add_limit_order(Order&& order){
    const TimeInForce tif = order.get_time_in_force();
    
    if(tif == TimeInForce::FillOrKill && !can_fully_fill(order)){
        return false;
    }
    
    if(order.is_buy()){
        match_buy(order);
    }
    else{
        match_sell(order);
    }
    
    if(tif == TimeInForce::GoodTillCancel){
        if(order.is_buy()){
            return add_order_to_side(bids_, std::move(order));
        }
        return add_order_to_side(asks_, std::move(order));
    }
    
    return true;
}

bool OrderBook::add_market_order(Order&& order){
    //buy 

    //sell
}

bool OrderBook::add_order(Order order){ //need improvement
    //add price level if not exist
    if(order.is_limit_order()){
        return add_limit_order(std::move(order));
    }
    return add_market_order(std::move(order));
}

void OrderBook::match_buy(Order& buy_order){ //need improvement
    match_against_side(buy_order, asks_, cmp_buy);
}

void OrderBook::match_sell(Order& sell_order){ //need improvement
    match_against_side(sell_order, bids_, cmp_sell);
}

void OrderBook::execute_trade(Order& incoming_order, PriceLevel& counterpart_level){
    //execute min quantity of two
    Quantity executed_quantity = std::min(
        incoming_order.get_remaining_quantity(),
        counterpart_level.front_order().get_remaining_quantity()
    );

    Trade trade = Trade::from_match(incoming_order, counterpart_level.front_order());
    trades_.push_back(trade); //need improvment: preallocate

    incoming_order.fill_order(executed_quantity);
    counterpart_level.fill_front_order(executed_quantity);
}

bool OrderBook::cancel_order(OrderId order_id){
    auto it = order_look_up_.find(order_id);
    if(it == order_look_up_.end()){
        return false;
    }
    
    OrderLocation& order_location = it->second;
    const Side& side = order_location.side_;

    if(side == Side::Buy){
        return cancel_order_side(bids_, order_location);
    }
    
    return cancel_order_side(asks_, order_location);
}




//debug method

std::string OrderBook::to_string() const {
    std::string output = "================ ORDER BOOK ================\n";
    output += "Asks (Sellers):\n";
    output += levels_to_string(asks_);
    output += "Bids (Buyers):\n";
    output += levels_to_string(bids_);
    output += "============================================\n";

    return output;
}

std::string OrderBook::trades_to_string() const {
    std::string output = "";
    for(const Trade& trade : trades_){
        output += trade.to_string();
    }

    return output; 
}

std::ostream& operator << (std::ostream& os, const OrderBook& order_book){
    os << order_book.to_string();
    return os;
}
