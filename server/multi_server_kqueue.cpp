#include "networking/event_sender.h"
#include "networking/msg_parser.h"
#include "networking/transport/tcp.h"
#include "networking/wait_mode.h"
#include "orderbook/order_book.h"
#include "engine/message_consumer.h"
#include <unistd.h>
#include <iostream>
#include <thread>
#include <mutex>

int main(){

    EventSink event_sink;
    MessageSink msg_sink;
    OrderBook<EventSink> book(event_sink);
    ClientStateList fd_to_state;
    std::mutex fd_to_state_mutex;
    bool block_mode = read_block_mode_from_env();

    AtomicBool running = true;

    //engine thread drains message from sink then orderbook matches
    std::thread engine_thread(
        consume_messages,
        std::reference_wrapper(book),
        std::reference_wrapper(msg_sink),
        std::reference_wrapper(running),
        block_mode
    );
    std::thread event_sender_thread(
        run_sender,
        std::reference_wrapper(fd_to_state),
        std::reference_wrapper(fd_to_state_mutex),
        std::reference_wrapper(event_sink),
        std::reference_wrapper(running),
        block_mode
    );

    if(!setup_server(msg_sink, fd_to_state, fd_to_state_mutex)){
        std::cerr << "Error occured!\n";
        return 1;
    }
    event_sender_thread.join();
    engine_thread.join();
}
