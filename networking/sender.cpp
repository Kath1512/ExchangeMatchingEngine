#include "networking/sender.h"


void run_sender(int fd, DefaultSink& sink, AtomicBool& running){
    Event item;
    while (running || !sink.empty()) {
        bool success = sink.pop(item);
        if(!success) continue;

        bool ok = std::visit(overloaded{
            [fd](const OrderAccepted& event)  { return send_event<WireOrderAccepted>(fd, event); },
            [fd](const OrderRejected& event)  { return send_event<WireOrderRejected>(fd, event); },
            [fd](const OrderCanceled& event)  { return send_event<WireOrderCanceled>(fd, event); },
            [fd](const CancelRejected& event) { return send_event<WireCancelRejected>(fd, event); },
            [fd](const OrderModified& event)  { return send_event<WireOrderModified>(fd, event); },
            [fd](const ModifyRejected& event) { return send_event<WireModifyRejected>(fd, event); },
            [fd](const TradeExecuted& event)  { return send_event<WireTradeExecuted>(fd, event); }
        }, item);
        if(!ok) break;
    }
}