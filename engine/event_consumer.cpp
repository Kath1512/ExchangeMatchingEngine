#include "engine/event_consumer.h"

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void consume_events(EventSink& sink, AtomicBool& running, bool block_mode) {
    Event item;
    while (running || !sink.empty()) {
        bool got = block_mode
            ? sink.wait_pop(item, std::chrono::milliseconds(50))
            : sink.pop(item);
        if(!got) continue;

        std::visit(overloaded{
            [](const RoutedEvent& ev) {
                std::visit([&](auto&& inner){ std::cout << "[PRIVATE conn=" << ev.connection_id << "] " << inner << "\n"; }, ev.event);
            },
            [](const PublicEvent& ev) {
                std::visit([](auto&& inner){ std::cout << "[PUBLIC] " << inner << "\n"; }, ev);
            }
        }, item);
    }
}
