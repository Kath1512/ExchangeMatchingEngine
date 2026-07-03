#pragma once

#include "order_book.h"
#include "ring_buffer.h"
#include "constant.h"

using DefaultSink = RingBuffer<Event, RING_BUFFER_SIZE>;
// using DefaultBook = OrderBook<DefaultSink>;

void consume_events(DefaultSink& sink, AtomicBool& running); //no need to drain from the book, just drain from the ring buffer
