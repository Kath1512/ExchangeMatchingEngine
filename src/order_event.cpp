#include "orderbook/order_event.h"



std::ostream& operator << (std::ostream& os, const OrderAccepted& ev){
    os << std::format("Accepted order {}", ev.order_id);
    return os;
}

std::ostream& operator << (std::ostream& os, const OrderRejected& ev){
    os << std::format(
        "Rejected order {} | Reason: {}",
        ev.order_id,
        to_string(ev.reason)
    );
    return os;
}

std::ostream& operator << (std::ostream& os, const OrderCanceled& ev){
    os << std::format("Cancelled order {}", ev.order_id);
    return os;
}

std::ostream& operator << (std::ostream& os, const CancelRejected& ev){
    os << std::format(
        "Rejected cancelling order {} | Reason: {}",
        ev.order_id,
        to_string(ev.reason)
    );
    return os;
}

std::ostream& operator << (std::ostream& os, const OrderModified& ev){
    os << std::format(
        "Modified order {} with new price: {}, new quantity: {}", 
        ev.order_id,
        ev.new_price,
        ev.new_quantity
    );
    return os;
}

std::ostream& operator << (std::ostream& os, const ModifyRejected& ev){
    os << std::format(
        "Rejected modifying order {} | Reason: {}",
        ev.order_id,
        to_string(ev.reason)
    );
    return os;
}

std::ostream& operator << (std::ostream& os, const TradeExecuted& ev){
    os << std::format(
            "Trade executed between sell order {} and buy order {} at price: {}, quantity: {}",
            ev.seller_id,
            ev.buyer_id,
            ev.price,
            ev.quantity
    );

    return os;
}

std::string to_string(RejectReason reason){
    switch(reason){
        case RejectReason::NoLiquidity:      return "NoLiquidity";
        case RejectReason::InvalidOrderId:   return "InvalidOrderId";
        case RejectReason::DuplicateOrderId: return "DuplicateOrderId";
        case RejectReason::InvalidParams:    return "InvalidParams";
        case RejectReason::Unknown:          return "Unknown";
    }
    return "Unknown";
}
