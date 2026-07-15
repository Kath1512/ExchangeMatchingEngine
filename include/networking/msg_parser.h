#pragma once
#include <atomic>

#include "networking/protocol.h"
#include "networking/socket_utils.h"
#include "engine/event_consumer.h"
#include "networking/client_state.h"
#include "networking/constant.h"

using MaybeMessage = std::optional<Message>;
using MessageSink = RingBuffer<Message, RING_BUFFER_SIZE>;

// void run_parser(int fd, MessageSink& sink, AtomicBool& running, int connection_id);
int parse_ready_client(
    ClientState& state,
    MessageSink& sink,
    int connection_id
);
