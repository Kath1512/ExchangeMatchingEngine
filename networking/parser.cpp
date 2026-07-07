#include "networking/parser.h"

template<typename WireT, typename EventT>
MaybeEvent recv_event(int fd){
    std::array<uint8_t, sizeof(WireT)> buf;
    auto got = recv_all(fd, buf.data(), buf.size());
    if(got == -1){
        return std::nullopt;
    }

    if(got == 0){
        return std::nullopt;
    }

    EventT event;
    bool ok = deserialise(buf.data(), event);
    if(!ok){
        return std::nullopt;
    }

    return event;
}
void run_parser(int fd, DefaultSink& sink, AtomicBool& running){
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
                MaybeEvent e = recv_event<WireOrderRested, OrderRested>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::OrderFilled: {
                MaybeEvent e = recv_event<WireOrderFilled, OrderFilled>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::OrderExpired: {
                MaybeEvent e = recv_event<WireOrderExpired, OrderExpired>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::OrderRejected: {
                MaybeEvent e = recv_event<WireOrderRejected, OrderRejected>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::OrderCanceled: {
                MaybeEvent e = recv_event<WireOrderCanceled, OrderCanceled>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::CancelRejected: {
                MaybeEvent e = recv_event<WireCancelRejected, CancelRejected>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::OrderModified: {
                MaybeEvent e = recv_event<WireOrderModified, OrderModified>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::ModifyRejected: {
                MaybeEvent e = recv_event<WireModifyRejected, ModifyRejected>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            case MsgType::TradeExecuted: {
                MaybeEvent e = recv_event<WireTradeExecuted, TradeExecuted>(fd);
                if(e) sink.push(std::move(e.value()));
                break;
            }
            default:
                std::cerr << "Unknown message type\n";
                break;
        }
        
    }
}
