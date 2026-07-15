#include "networking/event_sender.h"
#include "networking/msg_parser.h"
#include "networking/transport/tcp.h"
#include "orderbook/order_book.h"
#include "engine/message_consumer.h"
#include <unistd.h>
#include <iostream>
#include <thread>

int main(){
    int socket_fd, client_fd;
    if(!setup_server(socket_fd, client_fd)){
        std::cerr << "Error occured!\n";
        return 1;
    }

    EventSink event_sink;
    MessageSink msg_sink;
    OrderBook<EventSink> book(event_sink);

    AtomicBool running = true;
    std::thread msg_parser_thread(
        run_parser,
        client_fd,
        std::reference_wrapper(msg_sink),
        std::reference_wrapper(running),
        client_fd
    ); // this need to use epoll for multiple clients

    //engine thread drains message from sink then orderbook matches
    std::thread engine_thread(
        consume_messages,
        std::reference_wrapper(book),
        std::reference_wrapper(msg_sink),
        std::reference_wrapper(running)
    );
    std::thread event_sender_thread(
        run_sender,
        client_fd,
        std::reference_wrapper(event_sink),
        std::reference_wrapper(running)
    );

    event_sender_thread.join();
    engine_thread.join();
    msg_parser_thread.join();
    close(client_fd);
    close(socket_fd);
}
