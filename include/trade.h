#pragma once
#include <string>
#include <format>
#include <ostream>
#include "types.h"
#include "order.h"
#include <vector>


struct TradeInfo{
    OrderId order_id_;
    Price price_;
    Quantity quantity_;

    TradeInfo() = delete;
    TradeInfo(OrderId order_id, Price price, Quantity quantity);
    TradeInfo(const Order& order);

    OrderId get_order_id() const;
    Price get_price() const;
    Quantity get_quantity() const;
};

class Trade{
private:
    TradeInfo buyer_;
    TradeInfo seller_;

    Price execution_price_;
    Quantity execution_quantity_;

    Side aggressor_side_;
public:

    Trade() = delete;

    static Trade from_match(const Order& incoming, const Order& resting);

    Trade(const TradeInfo& buyer, 
          const TradeInfo& seller, 
          Price execution_price,
          Quantity execution_quantity,
          Side aggressor_side);

    const TradeInfo& get_buyer() const;
    const TradeInfo& get_seller() const;

    Price get_execution_price() const;
    Quantity get_execution_quantity() const;
    Side get_aggressor_side() const;

    //debug method
    std::string to_string() const;

    friend std::ostream& operator<< (std::ostream& os, const Trade& trade);

};

std::ostream& operator << (std::ostream& os, const std::vector<Trade>& trades);