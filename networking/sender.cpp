#include "networking/sender.h"


void run_sender(int fd, DefaultSink& sink, AtomicBool& running){
    Event item;
    while (running || !sink.empty()) {
        bool success = sink.pop(item);
        if(!success) continue;

        bool ok = std::visit(overloaded{
            [fd](const OrderRested& event)    { return send_event<WireOrderRested>(fd, event); },
            [fd](const OrderFilled& event)    { return send_event<WireOrderFilled>(fd, event); },
            [fd](const OrderExpired& event)   { return send_event<WireOrderExpired>(fd, event); },
            [fd](const OrderRejected& event)  { return send_event<WireOrderRejected>(fd, event); },
            [fd](const OrderCanceled& event)  { return send_event<WireOrderCanceled>(fd, event); },
            [fd](const CancelRejected& event) { return send_event<WireCancelRejected>(fd, event); },
            [fd](const OrderModified& event)  { return send_event<WireOrderModified>(fd, event); },
            [fd](const ModifyRejected& event) { return send_event<WireModifyRejected>(fd, event); },
            [fd](const TradeExecuted& event)  { return send_event<WireTradeExecuted>(fd, event); },
            [fd](const BookUpdate& event)     { return send_event<WireBookUpdate>(fd, event); }
        }, item);
        if(!ok) break;
    }
}