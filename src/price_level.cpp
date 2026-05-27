#include "price_level.h"
#include <optional>

PriceLevel::PriceLevel(Price price)
    : price_(price), total_quantity_(0) {
        if(price <= 0){
            throw std::logic_error("Price level cannot be smaller than or equal to 0");
        }
    }

Price PriceLevel::get_price() const {
    return price_;
}

Quantity PriceLevel::get_total_quantity() const {
    return total_quantity_;
}

std::size_t PriceLevel::get_order_count() const {
    return orders_.size();
}

bool PriceLevel::empty() const {
    return orders_.empty();
}

MaybeOrderIterator PriceLevel::add_order(Order&& order) { //need improvement move, copy constructor
    //matched price
    if(order.get_price() != price_){
        throw std::runtime_error(std::format(
            "unmatched price level of {} for order id {} with price {}",
            price_, order.get_order_id(), order.get_price()
        ));
    }
    //order remaining quantity must not be 0
    if(order.is_filled()){
        throw std::runtime_error("Cannot add already filled order");
    }

    total_quantity_ += order.get_remaining_quantity();
    orders_.push_back(std::move(order));

    return std::prev(orders_.end());
}

void PriceLevel::erase_order(OrderIterator it){
    total_quantity_ -= it->get_remaining_quantity();
    orders_.erase(it);
}

void PriceLevel::pop_front_order(){
    if(orders_.empty()){
        throw std::logic_error(std::format("Price level {} is currently empty", price_));
    }

    if(!orders_.front().is_filled()){
        throw std::logic_error(std::format("The front order of this price level has not been filled, hence cannot be popped"));
    }

    orders_.pop_front();
}

void PriceLevel::fill_front_order(Quantity fill_quantity){
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

Order& PriceLevel::front_order(){
    return orders_.front();
}

const Order& PriceLevel::front_order() const {
    return orders_.front();
}

std::string PriceLevel::to_string() const {
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

std::ostream& operator << (std::ostream& os, const PriceLevel& price_level){
    os << price_level.to_string();
    return os;
}
