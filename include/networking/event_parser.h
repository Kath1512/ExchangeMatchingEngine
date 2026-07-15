#pragma once
#include <atomic>

#include "networking/protocol.h"
#include "networking/socket_utils.h"
#include "engine/event_consumer.h"

using MaybeEvent = std::optional<Event>;

void run_parser(int fd, EventSink& sink, AtomicBool& running);
