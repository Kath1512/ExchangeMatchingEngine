#pragma once

#include "orderbook/order_book.h"
#include "common/ring_buffer.h"
#include "engine/constant.h"

using EventSink = RingBuffer<Event, RING_BUFFER_SIZE>;
using MessageSink = RingBuffer<Message, RING_BUFFER_SIZE>;
using DefaultBook = OrderBook<EventSink>;

void consume_messages(
    DefaultBook& book,
    MessageSink& sink,
    AtomicBool& running,
    bool block_mode
); //no need to drain from the book, just drain from the ring buffer
