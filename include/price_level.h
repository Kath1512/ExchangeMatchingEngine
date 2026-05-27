#pragma once
#include <list>
#include <string>
#include <format>
#include <ostream>
#include <stdexcept>
#include <cstddef>
#include <optional>

#include "types.h"
#include "order.h"

using OrderList = std::list<Order>;
using OrderIterator = std::list<Order>::iterator; 
using MaybeOrderIterator = std::optional<OrderList::iterator>;

class PriceLevel{
private:
    Price price_;
    Quantity total_quantity_;
    OrderList orders_;
public:
    PriceLevel() = delete;

    explicit PriceLevel(Price price);

    Price get_price() const;
    Quantity get_total_quantity() const;
    std::size_t get_order_count() const;
    bool empty() const;

    MaybeOrderIterator add_order(Order&& order); //need improvement move, copy constructor
    void pop_front_order();
    void fill_front_order(Quantity fill_quantity);
    // bool cancel_order();
    void erase_order(OrderList::iterator it);

    Order& front_order();
    const Order& front_order() const;

    std::string to_string() const;

    friend std::ostream& operator << (std::ostream& os, const PriceLevel& price_level);
};
