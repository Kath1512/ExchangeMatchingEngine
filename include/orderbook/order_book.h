#pragma once
#include <map>
#include <functional>
#include <string>
#include <format>
#include <ostream>
#include <stdexcept>
#include <algorithm>
#include <iostream>
// #include <mutex>
// #include <condition_variable>

#include "types.h"
#include "order.h"
#include "price_level.h"
#include "trade.h"
#include "order_messages.h"
#include "order_event.h"


struct OrderLocation {
    Side side_;
    Price price_;
    OrderIterator order_it_;
};

struct AddOrderResult {
    AddOrderStatus status_;
    Quantity execution_quantity_;
    Quantity remaining_quantity_;

    static AddOrderResult from_match(
        OrderType order_type,
        Quantity execution_quantity,
        Quantity remaining_quantity) {

        if(remaining_quantity == 0){
            return AddOrderResult {
                AddOrderStatus::FullyFilled,
                execution_quantity,
                remaining_quantity
            };
        }

        if(execution_quantity == 0){
            if(order_type == OrderType::Market){
                return AddOrderResult {
                    AddOrderStatus::Rejected,
                    execution_quantity,
                    remaining_quantity
                };
            }
            return AddOrderResult {
                AddOrderStatus::Resting,
                execution_quantity,
                remaining_quantity
            };
        }

        return AddOrderResult {
                AddOrderStatus::PartiallyFilled,
                execution_quantity,
                remaining_quantity
        };
    }

    static AddOrderResult from_match(
        bool success, 
        OrderType order_type,
        Quantity execution_quantity,
        Quantity remaining_quantity){

        if(success){
            return AddOrderResult::from_match(
                order_type,
                execution_quantity, 
                remaining_quantity
            );
        }
        else{
            return AddOrderResult{
                AddOrderStatus::Failed,
                0,
                remaining_quantity
            };
        }
    }

    std::string to_string() const {
        return std::format(
            "Order status: {}, execution quantity: {}, "
            "remaining quantity: {}",
            ::to_string(status_),
            execution_quantity_,
            remaining_quantity_
        );
    }
};

using TradeHistory = std::vector<Trade>;
using AskLevels = std::map<Price, PriceLevel>;
using BidLevels = std::map<Price, PriceLevel, std::greater<Price>>;
using OrderLookUp = std::unordered_map<OrderId, OrderLocation>;

template<typename EventSink>
class OrderBook{
private:
    BidLevels bids_;
    AskLevels asks_;
    TradeHistory trades_;
    OrderLookUp order_look_up_;
    uint64_t next_order_id_ = 1;
    // std::condition_variable event_cv_;
    EventSink& event_sink_;

    static inline bool cmp_buy(const Price buy, const Price sell){
        return buy >= sell;
    }

    static inline bool cmp_sell(const Price sell, const Price buy){
        return sell <= buy;
    }

    
public:
    OrderBook(EventSink& event_sink);
    bool is_empty() const;
    bool has_bids() const;
    bool has_asks() const;
    
    Price best_bid() const;
    Price best_ask() const;
    const TradeHistory& get_trades() const;
    
    // ── Level inspection (used by the test framework) ─────────────────────
    bool has_level(Side side, Price price) const;
    Quantity level_quantity(Side side, Price price) const;
    
    std::size_t level_order_count(Side side, Price price) const;
    std::size_t bid_level_count() const;
    std::size_t ask_level_count() const;

    template<typename BookLevelsSide>
    bool add_order_to_side(BookLevelsSide& levels, Order&& order){ //need improvement, need more informed result instead of fail/pass
        if(order.is_filled()) return true; 

        Price price = order.get_price();
        Side side = order.get_side();

        auto [it, inserted_in_level] = levels.try_emplace(price, price);

        MaybeOrderIterator order_it = it->second.add_order(std::move(order));
        auto [look_up_it, inserted_in_look_up] = order_look_up_.try_emplace(
            order.get_order_id(),
            order.get_side(),
            order.get_price(),
            order_it.value()
        );

        add_public_event(BookUpdate{ side, price, it->second.get_total_quantity() });

        return inserted_in_look_up;
    }

    template<typename BookLevelsSide, typename Compare>
    Quantity match_limit_order_against_side(Order& order, BookLevelsSide& levels, Compare comp){
        //scan from front of asks -> end
        const Price price = order.get_price();
        Quantity execution_quantity = 0;

        if(levels.empty()) return 0;

        auto levels_iter = levels.begin();
        while(!order.is_filled()
            && levels_iter != levels.end()
            && comp(price, levels_iter->second.get_price())){

                PriceLevel& current_counterpart_level = levels_iter->second;
                while(!order.is_filled() && !current_counterpart_level.empty()){
                    execution_quantity += execute_trade(order, current_counterpart_level);
                }

                if(levels_iter->second.empty()){
                    levels_iter = levels.erase(levels_iter);
                }
        }

        return execution_quantity;
    }

    template<typename BookLevelSide>
    Quantity match_market_order_against_side(BookLevelSide& levels, Order& order){
        Quantity execution_quantity = 0;

        if(levels.empty()){
            return 0;
        }

        auto levels_iter = levels.begin();
        while(!order.is_filled()
            && levels_iter != levels.end()){

                PriceLevel& current_counterpart_level = levels_iter->second;
                while(!order.is_filled() && !current_counterpart_level.empty()){
                    execution_quantity += execute_trade(order, current_counterpart_level);
                }

                if(levels_iter->second.empty()){
                    levels_iter = levels.erase(levels_iter);
                }
        }

        return execution_quantity;
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
        Quantity new_qty = level.empty() ? 0 : level.get_total_quantity();
        if(level.empty()){
            levels.erase(level_it);
        }

        add_public_event(BookUpdate{ side, price, new_qty });

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
    AddOrderResult add_order(Order order); //need improvement
    AddOrderResult add_limit_order(Order&& order);
    AddOrderResult add_market_order(Order&& order);
    Quantity match_buy(Order& buy_order);
    Quantity match_sell(Order& sell_order);
    Quantity execute_trade(Order& order, PriceLevel& counterpart_level);
    bool cancel_order(OrderId order_id);
    bool modify_order(OrderId order_id, Price new_price, Quantity new_quantity);
    bool remove_look_up(OrderId order_id);
    
    MaybeOrderRef find_order(OrderId order_id) const;

    
    void process(const AddOrder& add_order_msg);
    
    void process(const ModifyOrder& modify_order_msg);
    
    void process(const CancelOrder& cancel_order_msg);
    
    void process_message(const Message& msg);

    //Event control
    bool add_private_event(int connection_id, PrivateEvent event);
    bool add_public_event(PublicEvent event);

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

    template<typename ES>
    friend std::ostream& operator<<(std::ostream& os, const OrderBook<ES>& book);
};

std::ostream& operator << (std::ostream& os, const AddOrderResult& result);

#include "order_book.tpp"