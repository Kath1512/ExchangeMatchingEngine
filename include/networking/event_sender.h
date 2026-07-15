#pragma once
#include <atomic>
#include <mutex>

#include "networking/protocol.h"
#include "networking/socket_utils.h"
#include "engine/event_consumer.h"
#include "networking/client_state.h"
#include "common/overloaded.h"

using ClientStateList = std::unordered_map<int, ClientState>;

constexpr int PUBLIC = -1;

template<typename WireT, typename EventT>
bool send_event_private(int fd, const EventT& event){
    std::array<uint8_t, sizeof(MsgHeader) + sizeof(WireT)> buf;
    serialise(event, buf.data());
    return send_all(fd, buf.data(), buf.size()) != -1;
}

template<typename WireT, typename EventT>
bool send_event_public(ClientStateList& states, std::mutex& state_mutex, const EventT& event){
    std::array<uint8_t, sizeof(MsgHeader) + sizeof(WireT)> buf;
    serialise(event, buf.data());
    bool all_ok = true;
    std::lock_guard<std::mutex> lock(state_mutex);
    for(const auto& [fd, _] : states){
        if(send_all(fd, buf.data(), buf.size()) == -1){
            all_ok = false;
        }
    }
    return all_ok;
}

void run_sender(ClientStateList& state, std::mutex& state_mutex, EventSink& sink, AtomicBool& running, bool block_mode);
