#pragma once
#include "orderbook/order_event.h"
#include "orderbook/order_book.h"
#include <vector>

// Simple sink for testing — stores events in a vector for easy inspection.
struct VectorSink {
    std::vector<Event> events;

    bool push(Event e) {
        events.push_back(std::move(e));
        return true;
    }

    bool pop(Event& out) {
        if (events.empty()) return false;
        out = std::move(events.front());
        events.erase(events.begin());
        return true;
    }

    bool empty() const { return events.empty(); }
    std::size_t size() const { return events.size(); }
};

using TestBook = OrderBook<VectorSink>;
