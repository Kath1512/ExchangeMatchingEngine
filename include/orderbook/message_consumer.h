#pragma once

#include "order_book.h"
#include "ring_buffer.h"
#include "constant.h"

using EventSink = RingBuffer<Event, RING_BUFFER_SIZE>;
using MessageSink = RingBuffer<Message, RING_BUFFER_SIZE>;
using DefaultBook = OrderBook<EventSink>;

void consume_messages(
    DefaultBook& book,
    MessageSink& sink,
    AtomicBool& running,
    bool block_mode
); //no need to drain from the book, just drain from the ring buffer
