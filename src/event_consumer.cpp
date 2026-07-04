#include "orderbook/event_consumer.h"

void consume_events(DefaultSink& sink, AtomicBool& running) {
    Event item;
    // int cnt = 0;
    while (running || !sink.empty()) {
        bool success = sink.pop(item);
        // cnt++;

        if(!success) continue;

        std::visit([](auto&& event) {
            std::cout << event << "\n";
        }, item);
    }

    // std::cout << "Total loops: " << cnt << "\n";
}
