#include "orderbook/message_consumer.h"

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void consume_messages(
    DefaultBook& book,
    MessageSink& sink,
    AtomicBool& running,
    bool block_mode
) {
    while (running || !sink.empty()) {
        Message item;
        bool got = block_mode
            ? sink.wait_pop(item, std::chrono::milliseconds(50))
            : sink.pop(item);
        if(!got) continue;

        std::visit([&](auto&& msg){
            book.process(msg);
        }, item);
    }
}
