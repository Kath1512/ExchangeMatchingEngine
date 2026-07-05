#pragma once
#include "orderbook/order_messages.h"
#include <optional>

// Reads one message from stdin. Returns nullopt on -1 (quit) or bad input.
std::optional<Message> read_message();
