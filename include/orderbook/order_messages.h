#pragma once 

#include "types.h"
#include <variant>

struct AddOrder {
    OrderType order_type;
    TimeInForce time_in_force;
    OrderId client_order_id;
    Price price;
    Quantity quantity;
    Side side;
    int connection_id = -1;
    SequenceNumber seq = 1;
};

struct CancelOrder {
    OrderId id;
    int connection_id = -1;
    SequenceNumber seq = 1;
};

struct ModifyOrder {
    OrderId id;
    Price new_price;
    Quantity new_quantity;
    int connection_id = -1;
    SequenceNumber seq = 1;
};

using Message = std::variant<AddOrder, CancelOrder, ModifyOrder>;
