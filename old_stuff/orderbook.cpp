#include <iostream>
#include <vector>
#include <algorithm>
#include <format>
#include <string>
#include <ostream>
#include <list>
#include <cstddef>
#include <map>
#include <functional>
//alias
using OrderId = std::uint64_t;
using SequenceNumber = std::uint64_t;
using Price = std::int64_t;
using Quantity = std::int64_t;

enum class Side{
    Buy,
    Sell
};

//view enum string
constexpr std::string_view to_string(Side side){
    switch(side){
        case Side::Buy: 
            return "Buy";
        case Side::Sell: 
            return "Sell";
    }
    return "unknown";
}

enum class TimeInForce{
    FillOrKill,
    GoodTillCancel
};

constexpr std::string_view to_string(TimeInForce time_in_force){
    switch(time_in_force){
        case TimeInForce::FillOrKill: 
            return "FillOrKill";
        case TimeInForce::GoodTillCancel: 
            return "GoodTillCancel";
    }
    return "unknown";
}

class Order{
private:
    TimeInForce time_in_force_;
    OrderId order_id_;
    Price price_;
    Quantity initial_quantity_;
    Quantity remaining_quantity_;
    Side side_;
    SequenceNumber sequence_number_;
public:
    Order(TimeInForce time_in_force, 
          OrderId order_id, 
          Price price, 
          Quantity quantity, 
          Side side, 
          SequenceNumber sequence_number)
        :   time_in_force_(time_in_force), 
            order_id_(order_id),
            price_(price),
            initial_quantity_(quantity),
            remaining_quantity_(quantity),
            side_(side),
            sequence_number_(sequence_number){
                if(quantity <= 0){
                    throw std::logic_error("Order quantity cannot be smaller than or equal to 0");
                }

                if(price <= 0){
                    throw std::logic_error("Order price cannot be smaller than or equal to 0");
                }
            }

    TimeInForce get_time_in_force() const {
        return time_in_force_;
    }
    OrderId get_order_id() const {
        return order_id_;
    }
    Price get_price() const { 
        return price_;
    }
    Quantity get_initial_quantity() const {
        return initial_quantity_;
    }
    Quantity get_remaining_quantity() const {
        return remaining_quantity_;
    }
    Quantity get_filled_quantity() const {
        return initial_quantity_ - remaining_quantity_;
    }
    Side get_side() const {
        return side_;
    }
    SequenceNumber get_sequence_number() const {
        return sequence_number_;
    }

    bool is_filled() const {
        return (remaining_quantity_ == 0);
    }
    
    bool is_buy() const {
        return side_ == Side::Buy;
    }
    
    bool is_sell() const {
        return side_ == Side::Sell;
    }
    
    void fill_order(Quantity fill_quantity){
        if(fill_quantity > remaining_quantity_){
            throw std::logic_error((std::format(
                "Order {} cannot be filled since remaining quantity {} is smaller than filled quantity {}", 
                order_id_, remaining_quantity_, fill_quantity)));
        }

        remaining_quantity_ -= fill_quantity;
    }

    std::string to_string() const { //need improvement
        return std::format(
            "Order{{"
                "type = {}, "
                "id = {}, "
                "price = {}, "
                "initial quantity = {}, "
                "remaining quantity = {}, "
                "side = {}, "
                "sequence number = {}, "
                "filled = {}"
            "}}",
            ::to_string(time_in_force_), order_id_, price_,
            initial_quantity_, remaining_quantity_, ::to_string(side_),
            sequence_number_, is_filled()
        );
    }


    //cout method
    friend std::ostream& operator<< (std::ostream& os, const Order& order){
        os << order.to_string();
        return os;
    }
};


void test_order(){
    std::cout << "-----Test order-----\n";
    Order order(TimeInForce::FillOrKill, 1512, 15, 10, Side::Buy, 1);
    std::cout << order << "\n";
    order.fill_order(4);
    std::cout << order << "\n";
    order.fill_order(6);
    std::cout << order << "\n";

    std::cout << order.to_string();

    // Order order2(TimeInForce::FillOrKill, 1512, 15, 0, Side::Buy, 1);
}


using OrderList = std::list<Order>;

class PriceLevel{
private:
    Price price_;
    Quantity total_quantity_;
    OrderList orders_;
public:
    PriceLevel() = delete;

    explicit PriceLevel(Price price)
        : price_(price), total_quantity_(0) {
            if(price <= 0){
                throw std::logic_error("Price level cannot be smaller than or equal to 0");
            }
        }

    Price get_price() const {
        return price_;
    }

    Quantity get_total_quantity() const {
        return total_quantity_;
    }

    std::size_t get_order_count() const {
        return orders_.size();
    }

    bool empty() const {
        return orders_.empty();
    }

    void add_order(Order order) { //need improvement move, copy constructor
        //matched price
        if(order.get_price() != price_){
            throw std::runtime_error(std::format(
                "unmatched price level of {} for order id {} with price {}", 
                price_, order.get_order_id(), order.get_price()
            ));
        }
        //order remaining quantity must not be 0
        if(order.is_filled()){
            throw std::runtime_error(std::format(
                "Cannot add already filled order", 
                price_, order.get_order_id(), order.get_price()
            ));
        }

        orders_.push_back(std::move(order));
        total_quantity_ += order.get_initial_quantity();

    }

    void pop_front_order(){
        if(orders_.empty()){
            throw std::logic_error(std::format("Price level {} is currently empty", price_));
        }

        if(!orders_.front().is_filled()){
            throw std::logic_error(std::format("The front order of this price level has not been filled, hence cannot be popped"));
        }

        orders_.pop_front();
    }


    void fill_front_order(Quantity fill_quantity){
        if(orders_.empty()){
            throw std::logic_error(std::format("Price level {} is currently empty", price_));
        }

        orders_.front().fill_order(fill_quantity);
        total_quantity_ -= fill_quantity;
        //if front order is filled then pop
        if(orders_.front().is_filled()){
            pop_front_order();
        }
    }

    Order& front_order(){
        return orders_.front();
    }

    const Order& front_order() const {
        return orders_.front();
    }

    std::string to_string() const {
        std::string output = std::format("==================Price level {}==================\n", price_);
        output += std::format(
            "total quantity = {}  |   orders count = {}\n", 
            total_quantity_, 
            get_order_count()
        );

        int i = 0;
        for(auto order : orders_){
            output += std::format("Order {} -> {}\n", ++i, order.to_string());
        }

        output += "===============================================\n";
        return output;
    }

    friend std::ostream& operator << (std::ostream& os, const PriceLevel& price_level){
        os << price_level.to_string();
        return os;
    }
}; 


void test_price_level(){
    std::cout << "-----Test price level-----\n";
    PriceLevel pl(100);
    Order order1(TimeInForce::FillOrKill, 1, 100, 10, Side::Buy, 1);
    Order order2(TimeInForce::GoodTillCancel, 2, 100, 30, Side::Sell, 1);

    pl.add_order(order1);
    pl.add_order(order2);

    std::cout << pl << "\n";
    
    pl.fill_front_order(10);
    // pl.pop_front_order();

}


using AskLevels = std::map<Price, PriceLevel>;
using BidLevels = std::map<Price, PriceLevel, std::greater<Price>>;

class OrderBook{
private:
    BidLevels bids_;
    AskLevels asks_;
public:
    bool is_empty() const {
        return (bids_.empty() && asks_.empty());
    }

    bool has_bids() const {
        return !bids_.empty();
    }

    bool has_asks() const {
        return !asks_.empty();
    }

    Price best_bid() const {
        if(!has_bids()){
            throw std::runtime_error("Bids are empty");
        }

        return bids_.begin()->second.get_price();
    }

    Price best_ask() const {
        if(!has_asks()){
            throw std::runtime_error("Asks are empty");
        }

        return asks_.begin()->second.get_price();
    }

    template<typename BookLevelsSide>
    void add_order_to_side(BookLevelsSide& levels, Order& order){//need improvement
        Price price = order.get_price();

        auto [it, inserted] = levels.try_emplace(price, price);
        it->second.add_order(order);
    }


    void add_order(Order order){ //need improvement
        //add price level if not exist
        if(order.is_buy()){
            match_buy(order);
            if(!order.is_filled()){
                add_order_to_side(bids_, order);
            }
        }
        else{
            match_sell(order);
            if(!order.is_filled()){
                add_order_to_side(asks_, order);
            }
        }
    }

    void match_buy(Order& buy_order){
        //scan from front of asks -> end
        const Price buy_price = buy_order.get_price();

        if(!has_asks()) return;

        auto iter_asks = asks_.begin();
        while(!buy_order.is_filled() 
            && iter_asks != asks_.end()
            && buy_price >= iter_asks->second.get_price()){

                PriceLevel& current_bid_level = iter_asks->second;
                while(!buy_order.is_filled() && !current_bid_level.empty()){
                    execute_trade(buy_order, current_bid_level);
                }

                if(iter_asks->second.empty()){
                    iter_asks = asks_.erase(iter_asks);
                }
        }
    }

    void match_sell(Order& sell_order){
        const Price sell_price = sell_order.get_price();

        if(!has_bids()) return;

        auto iter_bids = bids_.begin();
        while(!sell_order.is_filled() 
            && iter_bids != bids_.end()
            && sell_price <= iter_bids->second.get_price()){

                PriceLevel& current_ask_level = iter_bids->second;
                while(!sell_order.is_filled() && !current_ask_level.empty()){
                    execute_trade(sell_order, current_ask_level);
                }

                if(iter_bids->second.empty()){
                    iter_bids = bids_.erase(iter_bids);
                }
        }
    }

    void execute_trade(Order& order, PriceLevel& counterpart_level){
        //execute min quantity of two
        Quantity executed_quantity = std::min(
            order.get_remaining_quantity(),
            counterpart_level.front_order().get_remaining_quantity()
        );

        Price executed_price = counterpart_level.front_order().get_price();

        order.fill_order(executed_quantity);
        counterpart_level.fill_front_order(executed_quantity);

    }

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

    std::string to_string() const {
        std::string output = "================ ORDER BOOK ================\n";
        output += "Asks (Sellers):\n";
        output += levels_to_string(asks_);
        output += "Bids (Buyers):\n";
        output += levels_to_string(bids_);
        output += "============================================\n";

        return output;
    }

    friend std::ostream& operator << (std::ostream& os, const OrderBook& order_book){
        os << order_book.to_string();
        return os;
    }

};

void test_order_book1(){
    OrderBook order_book;
    
    //bids
    Order order1(TimeInForce::FillOrKill, 1, 100, 10, Side::Buy, 1);
    order_book.add_order(order1);
    order_book.add_order(Order(TimeInForce::FillOrKill, 3, 120, 90, Side::Buy, 2));
    order_book.add_order(Order(TimeInForce::FillOrKill, 5, 150, 50, Side::Buy, 2));

    //asks
    Order order2(TimeInForce::GoodTillCancel, 2, 100, 30, Side::Sell, 1);
    order_book.add_order(order2);
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 4, 90, 70, Side::Sell, 3));
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 6, 230, 20, Side::Sell, 2));

    order_book.add_order(Order(TimeInForce::FillOrKill, 7, 120, 500, Side::Buy, 2));
    order_book.add_order(Order(TimeInForce::FillOrKill, 7, 90, 30, Side::Sell, 2));

    std::cout << order_book << "\n";
}

void test_order_book2(){
    OrderBook order_book;
    
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 1, 100, 10, Side::Sell, 1));
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 2, 100, 20, Side::Sell, 2));
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 3, 105, 15, Side::Sell, 3));
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 4, 110, 10, Side::Sell, 4));

    // Resting bids
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 5, 90, 10, Side::Buy, 5));
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 6, 85, 20, Side::Buy, 6));

    // Incoming buy sweeps asks
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 7, 105, 40, Side::Buy, 7));

    // Incoming sell sweeps bids
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 8, 80, 25, Side::Sell, 8));

    // No-crossing orders rest
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 9, 70, 5, Side::Buy, 9));
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 10, 120, 5, Side::Sell, 10));

    std::cout << order_book << "\n";
    
}

using TradeId = uint64_t;

struct Trade{
    TradeId trade_id_;
    Price price_;
    Quantity quantity_;
    OrderId buy_order_id_;
    OrderId sell_order_id_;
    
    Trade() = delete;
    Trade(OrderId buy_order_id, OrderId sell_order_id, Price price, Quantity quantity)
        : buy_order_id_(buy_order_id),
          sell_order_id_(sell_order_id),
          price_(price),
          quantity_(quantity) {}


    
    
    //debug method
    std::string to_string() const {
        return std::format(
            "Trade executed between sell order {} and buy order {} at price: {}, quantity: {}\n",
            buy_order_id_,
            sell_order_id_,
            price_,
            quantity_
        );
    }

    friend std::ostream& operator<< (std::ostream& os, const Trade& trade){
        os << trade.to_string();
        return os;
    }
};



int main(){
    // test_order();
    // test_price_level();
    test_order_book2();
}