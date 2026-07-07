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

template<typename T>
bool is_private(const Event& ev) {
    if(!std::holds_alternative<RoutedEvent>(ev)) return false;
    return std::holds_alternative<T>(std::get<RoutedEvent>(ev).event);
}

template<typename T>
const T& get_private(const Event& ev) {
    return std::get<T>(std::get<RoutedEvent>(ev).event);
}

template<typename T>
bool is_public(const Event& ev) {
    if(!std::holds_alternative<PublicEvent>(ev)) return false;
    return std::holds_alternative<T>(std::get<PublicEvent>(ev));
}

template<typename T>
const T& get_public(const Event& ev) {
    return std::get<T>(std::get<PublicEvent>(ev));
}
