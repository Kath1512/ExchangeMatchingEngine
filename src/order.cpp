#include "orderbook/order.h"

Order::Order(OrderType order_type,
             TimeInForce time_in_force,
             OrderId order_id,
             Price price,
             Quantity quantity,
             Side side,
             SequenceNumber sequence_number)
    :   order_type_(order_type),
        time_in_force_(time_in_force),
        order_id_(order_id),
        price_(price),
        initial_quantity_(quantity),
        remaining_quantity_(quantity),
        side_(side),
        sequence_number_(sequence_number){
            if(quantity <= 0){
                throw std::logic_error("Order quantity cannot be smaller than or equal to 0");
            }

            // if(price <= 0){
            //     throw std::logic_error("Order price cannot be smaller than or equal to 0");
            // }
        }

Order::Order(const AddOrder& msg)
        : order_type_(msg.order_type),
          time_in_force_(msg.time_in_force),
          order_id_(msg.order_id),
          price_(msg.price),
          initial_quantity_(msg.quantity),
          remaining_quantity_(msg.quantity),
          side_(msg.side) {}

Order::Order(const Order& other)
        : order_type_(other.get_order_type()),
        time_in_force_(other.get_time_in_force()),
        order_id_(other.get_order_id()),
        price_(other.get_price()),
        initial_quantity_(other.get_initial_quantity()),
        remaining_quantity_(other.get_remaining_quantity()),
        side_(other.get_side()) {}

OrderType Order::get_order_type() const {
    return order_type_;
}

TimeInForce Order::get_time_in_force() const {
    return time_in_force_;
}

OrderId Order::get_order_id() const {
    return order_id_;
}

Price Order::get_price() const {
    return price_;
}

Quantity Order::get_initial_quantity() const {
    return initial_quantity_;
}

Quantity Order::get_remaining_quantity() const {
    return remaining_quantity_;
}

Quantity Order::get_filled_quantity() const {
    return initial_quantity_ - remaining_quantity_;
}

Side Order::get_side() const {
    return side_;
}

SequenceNumber Order::get_sequence_number() const {
    return sequence_number_;
}

bool Order::is_filled() const {
    return (remaining_quantity_ == 0);
}

bool Order::is_buy() const {
    return side_ == Side::Buy;
}

bool Order::is_sell() const {
    return side_ == Side::Sell;
}

bool Order::is_limit_order() const {
    return order_type_ == OrderType::Limit;
}

bool Order::is_marker_order() const {
    return order_type_ == OrderType::Market;
}

void Order::fill_order(Quantity fill_quantity){
    if(fill_quantity > remaining_quantity_){
        throw std::logic_error((std::format(
            "Order {} cannot be filled since remaining quantity {} is smaller than filled quantity {}",
            order_id_, remaining_quantity_, fill_quantity)));
    }

    remaining_quantity_ -= fill_quantity;
}

std::string Order::to_string() const { //need improvement
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

std::ostream& operator<< (std::ostream& os, const Order& order){
    os << order.to_string();
    return os;
}
