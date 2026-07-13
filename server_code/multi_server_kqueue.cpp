#include "networking/event_sender.h"
#include "networking/msg_parser.h"
#include "networking/transport/tcp.h"
#include "orderbook/order_book.h"
#include "orderbook/message_consumer.h"
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

    AtomicBool running = true;
    // std::thread msg_parser_thread(
    //     run_parser,
    //     client_fd,
    //     std::reference_wrapper(msg_sink),
    //     std::reference_wrapper(running),
    //     client_fd
    // ); // this need to use epoll for multiple clients

    //engine thread drains message from sink then orderbook matches
    std::thread engine_thread(
        consume_messages,
        std::reference_wrapper(book),
        std::reference_wrapper(msg_sink),
        std::reference_wrapper(running)
    );
    std::thread event_sender_thread(
        run_sender,
        std::reference_wrapper(fd_to_state),
        std::reference_wrapper(fd_to_state_mutex),
        std::reference_wrapper(event_sink),
        std::reference_wrapper(running)
    );

    if(!setup_server(msg_sink, fd_to_state, fd_to_state_mutex)){
        std::cerr << "Error occured!\n";
        return 1;
    }
    event_sender_thread.join();
    engine_thread.join();
}
