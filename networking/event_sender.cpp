#include "networking/event_sender.h"


void run_sender(int fd, EventSink& sink, AtomicBool& running){
    Event item;
    while (running || !sink.empty()) {
        bool success = sink.pop(item);
        if(!success) continue;

        std::visit(overloaded{
            [](const RoutedEvent& ev) {
                std::visit([&](auto&& inner){ std::cout << "[PRIVATE conn=" << ev.connection_id << "] " << inner << "\n"; }, ev.event);
            },
            [](const PublicEvent& ev) {
                std::visit([](auto&& inner){ std::cout << "[PUBLIC] " << inner << "\n"; }, ev);
            }
        }, item);
        
        bool ok = std::visit(overloaded{
            [fd](const RoutedEvent& ev) {
                // Private: send to the originating client's fd
                // (For now single client, so fd == ev.connection_id fd — use fd directly)
                return std::visit(overloaded{
                    [fd](const OrderRested& e)    { return send_event<WireOrderRested>(fd, e); },
                    [fd](const OrderFilled& e)    { return send_event<WireOrderFilled>(fd, e); },
                    [fd](const OrderExpired& e)   { return send_event<WireOrderExpired>(fd, e); },
                    [fd](const OrderRejected& e)  { return send_event<WireOrderRejected>(fd, e); },
                    [fd](const OrderCanceled& e)  { return send_event<WireOrderCanceled>(fd, e); },
                    [fd](const CancelRejected& e) { return send_event<WireCancelRejected>(fd, e); },
                    [fd](const OrderModified& e)  { return send_event<WireOrderModified>(fd, e); },
                    [fd](const ModifyRejected& e) { return send_event<WireModifyRejected>(fd, e); }
                }, ev.event);
            },
            [fd](const PublicEvent& ev) {
                // Public: broadcast to all (single client for now)
                return std::visit(overloaded{
                    [fd](const TradeExecuted& e) { return send_event<WireTradeExecuted>(fd, e); },
                    [fd](const BookUpdate& e)    { return send_event<WireBookUpdate>(fd, e); }
                }, ev);
            }
        }, item);
        if(!ok) break;
    }
}