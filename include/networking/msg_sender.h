#pragma once
#include <atomic>

#include "networking/protocol.h"
#include "networking/socket_utils.h"
#include "engine/event_consumer.h"

template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...; 
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

template<typename WireT, typename MsgT>
bool send_msg(int fd, const MsgT& event){
    std::array<uint8_t, sizeof(MsgHeader) + sizeof(WireT)> buf;
    serialise(event, buf.data());
    return send_all(fd, buf.data(), buf.size()) != -1;
}

void run_sender(int fd, const Message& order_msg);
