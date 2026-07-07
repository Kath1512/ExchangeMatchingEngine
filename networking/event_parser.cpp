#include "networking/event_parser.h"

template<typename WireT, typename EventT>
static std::optional<EventT> recv_payload(int fd){
    std::array<uint8_t, sizeof(WireT)> buf;
    auto got = recv_all(fd, buf.data(), buf.size());
    if(got <= 0) return std::nullopt;
    EventT event;
    if(!deserialise(buf.data(), event)) return std::nullopt;
    return event;
}

template<typename WireT, typename EventT>
MaybeEvent recv_private(int fd){
    auto e = recv_payload<WireT, EventT>(fd);
    if(!e) return std::nullopt;
    return Event{ RoutedEvent{ -1, PrivateEvent{ std::move(*e) } } };
}

template<typename WireT, typename EventT>
MaybeEvent recv_public(int fd){
    auto e = recv_payload<WireT, EventT>(fd);
    if(!e) return std::nullopt;
    return Event{ PublicEvent{ std::move(*e) } };
}
void run_parser(int fd, EventSink& sink, AtomicBool& running){
    while(running){
        //receive header
        MsgHeader header;
        auto got = recv_all(fd, reinterpret_cast<uint8_t*>(&header), sizeof(MsgHeader));
        if(got == -1){
            std::cerr << "Error\n";
            break;
        }

        if(got == 0){
            std::cerr << "Connection closed\n";
            running = false;
            break;
        }
        auto [type, payload_len] = header;

        //receive payload
        
        switch (type){
            case MsgType::OrderRested: {
                MaybeEvent e = recv_private<WireOrderRested, OrderRested>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::OrderFilled: {
                MaybeEvent e = recv_private<WireOrderFilled, OrderFilled>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::OrderExpired: {
                MaybeEvent e = recv_private<WireOrderExpired, OrderExpired>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::OrderRejected: {
                MaybeEvent e = recv_private<WireOrderRejected, OrderRejected>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::OrderCanceled: {
                MaybeEvent e = recv_private<WireOrderCanceled, OrderCanceled>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::CancelRejected: {
                MaybeEvent e = recv_private<WireCancelRejected, CancelRejected>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::OrderModified: {
                MaybeEvent e = recv_private<WireOrderModified, OrderModified>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::ModifyRejected: {
                MaybeEvent e = recv_private<WireModifyRejected, ModifyRejected>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::TradeExecuted: {
                MaybeEvent e = recv_public<WireTradeExecuted, TradeExecuted>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::BookUpdate: {
                MaybeEvent e = recv_public<WireBookUpdate, BookUpdate>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            default:
                std::cerr << "Unknown message type\n";
                break;
        }
        
    }
}
