#pragma once 

#include "types.h"
#include <variant>

struct AddOrder {
    OrderType order_type;
    TimeInForce time_in_force;
    OrderId order_id;
    Price price;
    Quantity quantity;
    Side side;
    SequenceNumber seq = 1;
};

struct CancelOrder {
    OrderId id;
};

struct ModifyOrder {
    OrderId id;
    Price new_price;
    Quantity new_quantity;
};

using Message = std::variant<AddOrder, CancelOrder, ModifyOrder>;
