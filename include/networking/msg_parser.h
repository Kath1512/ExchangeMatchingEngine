#pragma once
#include <atomic>

#include "networking/protocol.h"
#include "networking/socket_utils.h"
#include "orderbook/event_consumer.h"

using MaybeMessage = std::optional<Message>;
using MessageSink = RingBuffer<Message, RING_BUFFER_SIZE>;

void run_parser(int fd, MessageSink& sink, AtomicBool& running, int connection_id);
