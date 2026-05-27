#pragma once
#include <map>
#include <functional>
#include <string>
#include <format>
#include <ostream>
#include <stdexcept>
#include <algorithm>

#include "types.h"
#include "order.h"
#include "price_level.h"
#include "trade.h"


struct OrderLocation{
    Side side_;
    Price price_;
    OrderIterator order_it_;
};

using TradeHistory = std::vector<Trade>;
using AskLevels = std::map<Price, PriceLevel>;
using BidLevels = std::map<Price, PriceLevel, std::greater<Price>>;
using OrderLookUp = std::unordered_map<OrderId, OrderLocation>;


class OrderBook{
private:
    BidLevels bids_;
    AskLevels asks_;
    TradeHistory trades_;
    OrderLookUp order_look_up_;

    static inline bool cmp_buy(const Price buy, const Price sell){
        return buy >= sell;
    }

    static inline bool cmp_sell(const Price sell, const Price buy){
        return sell <= buy;
    }

public:
    bool is_empty() const;
    bool has_bids() const;
    bool has_asks() const;

    Price best_bid() const;
    Price best_ask() const;
    const TradeHistory& get_trades() const;

    // ── Level inspection (used by the test framework) ─────────────────────
    bool        has_level(Side side, Price price) const;
    Quantity    level_quantity(Side side, Price price) const;
    std::size_t level_order_count(Side side, Price price) const;
    std::size_t bid_level_count() const;
    std::size_t ask_level_count() const;

    template<typename BookLevelsSide>
    bool add_order_to_side(BookLevelsSide& levels, Order&& order){ //need improvement
        if(order.is_filled()) return false;

        Price price = order.get_price();

        auto [it, inserted_in_level] = levels.try_emplace(price, price);

        MaybeOrderIterator order_it = it->second.add_order(order);
        auto [look_up_it, inserted_in_look_up] = order_look_up_.try_emplace(
            order.get_order_id(),
            order.get_side(),
            order.get_price(),
            order_it.value()
        );

        return inserted_in_look_up;
    }

    template<typename BookLevelsSide, typename Compare>
    void match_against_side(Order& order, BookLevelsSide& levels, Compare comp){
        //scan from front of asks -> end
        const Price price = order.get_price();

        if(levels.empty()) return;

        auto levels_iter = levels.begin();
        while(!order.is_filled()
            && levels_iter != levels.end()
            && comp(price, levels_iter->second.get_price())){

                PriceLevel& current_counterpart_level = levels_iter->second;
                while(!order.is_filled() && !current_counterpart_level.empty()){
                    execute_trade(order, current_counterpart_level);
                }

                if(levels_iter->second.empty()){
                    levels_iter = levels.erase(levels_iter);
                }
        }
    }

    template<typename BookLevelSide>
    bool cancel_order_side(BookLevelSide& levels, const OrderLocation& order_location){
        const auto& [side, price, order_it] = order_location;
        auto level_it = levels.find(price);
        if(level_it == levels.end()){
            return false;
        }
        PriceLevel& level = level_it->second;
        level.erase_order(order_it);
        //remove price level if empty
        if(level.empty()){
            levels.erase(level_it);
        }

        return true;
    }

    template<typename BookLevelSide, typename Compare>
    bool can_fully_fill_against_side(const BookLevelSide& levels, const Order& order, Compare comp) const {
        const Price price = order.get_price();
        Quantity quantity = order.get_remaining_quantity();

        if(levels.empty()) return false;

        auto levels_iter = levels.begin();
        while(quantity > 0
            && levels_iter != levels.end()
            && comp(price, levels_iter->second.get_price())){

                const PriceLevel& current_counterpart_level = levels_iter->second;
                quantity -= current_counterpart_level.get_total_quantity();

                levels_iter++;
        }

        return quantity <= 0;
    }
    

    bool can_fully_fill(const Order& order) const;
    bool add_order(Order order); //need improvement
    bool add_limit_order(Order&& order);
    bool add_market_order(Order&& order);
    void match_buy(Order& buy_order);
    void match_sell(Order& sell_order);
    void execute_trade(Order& order, PriceLevel& counterpart_level);
    bool cancel_order(OrderId order_id);

    //debug method
    template<typename Levels>
    std::string levels_to_string(Levels& levels) const {
        std::string output = "";
        for(auto [price, level] : levels){
            output += std::format(
                "{} -> total_quantity = {} | orders = {}\n",
                price,
                level.get_total_quantity(),
                level.get_order_count()
            );
        }

        return output;
    }

    std::string to_string() const;
    std::string trades_to_string() const;

    friend std::ostream& operator << (std::ostream& os, const OrderBook& order_book);
};
