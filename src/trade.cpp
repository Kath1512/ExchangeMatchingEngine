#include "trade.h"
#include "types.h"
#include "order.h"
#include <vector>

//TradeInfo implementation

TradeInfo::TradeInfo(OrderId order_id, Price price, Quantity quantity)
    : order_id_(order_id), price_(price), quantity_(quantity) {};

TradeInfo::TradeInfo(const Order& order)
    : order_id_(order.get_order_id()), price_(order.get_price()), quantity_(order.get_remaining_quantity()) {};


OrderId TradeInfo::get_order_id() const{
    return order_id_;
}
Price TradeInfo::get_price() const {
    return price_;
}
Quantity TradeInfo::get_quantity() const {
    return quantity_;
}

//Trade implementation
Trade::Trade(const TradeInfo& buyer, 
          const TradeInfo& seller, 
          Price execution_price,
          Quantity execution_quantity,
          Side aggressor_side
        ) : buyer_(buyer),
            seller_(seller),
            execution_price_(execution_price),
            execution_quantity_(execution_quantity),
            aggressor_side_(aggressor_side) {};

Trade Trade::from_match(const Order& incoming, const Order& resting){
        if(incoming.is_buy()){
            return Trade(
                TradeInfo(incoming),
                TradeInfo(resting),
                resting.get_price(),
                std::min(incoming.get_remaining_quantity(), resting.get_remaining_quantity()),
                incoming.get_side()
            );
        }
        else{
            return Trade(
                TradeInfo(resting),
                TradeInfo(incoming),
                resting.get_price(),
                std::min(incoming.get_remaining_quantity(), resting.get_remaining_quantity()),
                incoming.get_side()
            );
        }
    }
const TradeInfo& Trade::get_buyer() const {
    return buyer_;
}

const TradeInfo& Trade::get_seller() const {
    return seller_;
}

Price Trade::get_execution_price() const {
    return execution_price_;
}
Quantity Trade::get_execution_quantity() const {
    return execution_quantity_;
}
Side Trade::get_aggressor_side() const {
    return aggressor_side_;
}

std::string Trade::to_string() const {
    return std::format(
        "Trade executed between buy order {} and sell order {} at price: {}, quantity: {}\n",
        buyer_.order_id_,
        seller_.order_id_,
        execution_price_,
        execution_quantity_
    );
}

std::ostream& operator<< (std::ostream& os, const Trade& trade){
    os << trade.to_string();
    return os;
}

std::ostream& operator<< (std::ostream& os, const std::vector<Trade>& trades){
    for(const Trade& trade : trades){
        os << trade;
    }

    return os;
}
