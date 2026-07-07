#include "orderbook/message_consumer.h"

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void consume_messages(
    DefaultBook& book,
    MessageSink& sink, 
    AtomicBool& running
) {
    while (running || !sink.empty()) {
        Message item;
        if(!sink.pop(item)) continue;

        std::visit([&](auto&& msg){
            book.process(msg);
        }, item);
    }
}
