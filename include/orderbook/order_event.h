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

struct OrderRested {
    OrderId order_id;
    OrderId client_order_id;
    Quantity remaining_quantity;
};

struct OrderFilled {
    OrderId order_id;
    OrderId client_order_id;
};

struct OrderExpired {
    OrderId order_id;
    OrderId client_order_id;
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

struct BookUpdate {
    Side side;
    Price price;
    Quantity new_total_quantity;  // 0 means level is gone
};

std::ostream& operator << (std::ostream& os, const OrderRested& ev);
std::ostream& operator << (std::ostream& os, const OrderFilled& ev);
std::ostream& operator << (std::ostream& os, const OrderExpired& ev);

std::ostream& operator << (std::ostream& os, const OrderRejected& ev);

std::ostream& operator << (std::ostream& os, const OrderCanceled& ev);

std::ostream& operator << (std::ostream& os, const CancelRejected& ev);

std::ostream& operator << (std::ostream& os, const OrderModified& ev);

std::ostream& operator << (std::ostream& os, const ModifyRejected& ev);

std::ostream& operator << (std::ostream& os, const TradeExecuted& ev);
std::ostream& operator << (std::ostream& os, const BookUpdate& ev);

std::string to_string(RejectReason reason);

using PrivateEvent = std::variant<
    OrderRested,
    OrderFilled,
    OrderExpired,
    OrderRejected,
    OrderCanceled,
    CancelRejected,
    OrderModified,
    ModifyRejected
>;

using PublicEvent = std::variant<TradeExecuted, BookUpdate>;

struct RoutedEvent {
    int connection_id;
    PrivateEvent event;
};

std::ostream& operator << (std::ostream& os, const RoutedEvent& ev);

using Event = std::variant<RoutedEvent, PublicEvent>;