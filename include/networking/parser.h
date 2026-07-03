#pragma once
#include <atomic>

#include "networking/protocol.h"
#include "networking/socket_utils.h"
#include "orderbook/event_consumer.h"

using MaybeEvent = std::optional<Event>;

void run_parser(int fd, DefaultSink& sink, AtomicBool& running);
