#pragma once
#include <string>
#include <format>
#include <ostream>
#include <stdexcept>
#include <optional>
#include "types.h"
#include "helpers.h"
#include "order_messages.h"

class Order{
private:
    OrderType order_type_;
    TimeInForce time_in_force_;
    OrderId order_id_;
    Price price_;
    Quantity initial_quantity_;
    Quantity remaining_quantity_;
    Side side_;
    SequenceNumber sequence_number_;
    int connection_id_;
    OrderId client_order_id_;
public:
    static Order from_replacement(
        const Order& order,
        Price new_price,
        Quantity new_quantity
    ) {
        return Order(
            order.get_order_type(),
            order.get_time_in_force(),
            order.get_order_id(),
            new_price,
            new_quantity,
            order.get_side(),
            order.get_sequence_number(),
            order.get_connection_id(),
            order.get_client_order_id()
        );
    }

    Order(OrderType order_type,
          TimeInForce time_in_force,
          OrderId order_id,
          Price price,
          Quantity quantity,
          Side side,
          SequenceNumber sequence_number,
          int connection_id,
          OrderId client_order_id);

    Order(const Order& order);

    OrderType get_order_type() const;
    TimeInForce get_time_in_force() const;
    OrderId get_order_id() const;
    Price get_price() const;
    Quantity get_initial_quantity() const;
    Quantity get_remaining_quantity() const;
    Quantity get_filled_quantity() const;
    Side get_side() const;
    SequenceNumber get_sequence_number() const;
    int get_connection_id() const;
    OrderId get_client_order_id() const;

    bool is_filled() const;
    bool is_buy() const;
    bool is_sell() const;
    bool is_limit_order() const;
    bool is_marker_order() const;

    void fill_order(Quantity fill_quantity);

    std::string to_string() const;

    friend std::ostream& operator<< (std::ostream& os, const Order& order);
};

using MaybeOrderRef = std::optional<std::reference_wrapper<const Order>>;