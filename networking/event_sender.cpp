#include "networking/event_sender.h"


void run_sender(ClientStateList& state, std::mutex& state_mutex, EventSink& sink, AtomicBool& running, bool block_mode){
    Event item;
    while (running || !sink.empty()) {
        bool success = block_mode
            ? sink.wait_pop(item, std::chrono::milliseconds(50))
            : sink.pop(item);
        if(!success) continue;
        bool ok = std::visit(overloaded{
            [](const RoutedEvent& ev) {
                // Private: send to the originating client's client_fd
                int client_fd = ev.connection_id;
                return std::visit(overloaded{
                    [client_fd](const OrderRested& e)    { return send_event_private<WireOrderRested>(client_fd, e); },
                    [client_fd](const OrderFilled& e)    { return send_event_private<WireOrderFilled>(client_fd, e); },
                    [client_fd](const OrderExpired& e)   { return send_event_private<WireOrderExpired>(client_fd, e); },
                    [client_fd](const OrderRejected& e)  { return send_event_private<WireOrderRejected>(client_fd, e); },
                    [client_fd](const OrderCanceled& e)  { return send_event_private<WireOrderCanceled>(client_fd, e); },
                    [client_fd](const CancelRejected& e) { return send_event_private<WireCancelRejected>(client_fd, e); },
                    [client_fd](const OrderModified& e)  { return send_event_private<WireOrderModified>(client_fd, e); },
                    [client_fd](const ModifyRejected& e) { return send_event_private<WireModifyRejected>(client_fd, e); }
                }, ev.event);
            },
            [&state, &state_mutex](const PublicEvent& ev) {
                // Public: broadcast to all connected clients
                return std::visit(overloaded{
                    [&state, &state_mutex](const TradeExecuted& e) { return send_event_public<WireTradeExecuted>(state, state_mutex, e); },
                    [&state, &state_mutex](const BookUpdate& e)    { return send_event_public<WireBookUpdate>(state, state_mutex, e); }
                }, ev);
            }
        }, item);
        if(!ok){
            std::cerr << "Failed to deliver an event to one or more clients\n";
        }
    }
}