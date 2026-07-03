#pragma once
//add order event
//cancel order event
//modify order event
//Trade executed Event

#include "types.h"
#include <variant>
#include <ostream>
#include <format>

enum class RejectReason : uint8_t {
    NoLiquidity      = 1,
    InvalidOrderId   = 2,
    DuplicateOrderId = 3,
    InvalidParams    = 4,
    Unknown          = 255,
};

struct OrderAccepted {
    OrderId order_id;
};

struct OrderRejected {
    OrderId order_id;
    RejectReason reason;
};

struct OrderCanceled {
    OrderId order_id;
};

struct CancelRejected {
    OrderId order_id;
    RejectReason reason;
};

struct OrderModified {
    OrderId order_id;
    Price new_price;
    Quantity new_quantity;
};

struct ModifyRejected {
    OrderId order_id;
    RejectReason reason;
};

struct TradeExecuted {
    OrderId buyer_id;
    OrderId seller_id;
    Price price;
    Quantity quantity;
    Side aggressive_side;
};

std::ostream& operator << (std::ostream& os, const OrderAccepted& ev);

std::ostream& operator << (std::ostream& os, const OrderRejected& ev);

std::ostream& operator << (std::ostream& os, const OrderCanceled& ev);

std::ostream& operator << (std::ostream& os, const CancelRejected& ev);

std::ostream& operator << (std::ostream& os, const OrderModified& ev);

std::ostream& operator << (std::ostream& os, const ModifyRejected& ev);

std::ostream& operator << (std::ostream& os, const TradeExecuted& ev);

std::string to_string(RejectReason reason);

using Event = std::variant<
    OrderAccepted,
    OrderRejected,
    OrderCanceled,
    CancelRejected,
    OrderModified,
    ModifyRejected,
    TradeExecuted
>;