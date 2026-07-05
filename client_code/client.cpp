#include "networking/parser.h"
#include "networking/transport/tcp.h"
#include "orderbook/event_consumer.h"
#include <unistd.h>
#include <iostream>
#include <thread>

int main(){
    int socket_fd;
    if(!setup_client(socket_fd)){
        std::cerr << "Error occured!\n";
        return 1;
    }

    DefaultSink sink;
    AtomicBool running = true;
    std::thread recv_thread(
        run_parser,
        socket_fd,
        std::reference_wrapper(sink),
        std::reference_wrapper(running)
    );

    std::thread consumer_thread(
        consume_events,
        std::reference_wrapper(sink),
        std::reference_wrapper(running)
    );

    recv_thread.join();
    consumer_thread.join();
    close(socket_fd);
}
