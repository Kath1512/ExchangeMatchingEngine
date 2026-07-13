#include "networking/event_parser.h"
#include "networking/transport/tcp.h"
#include "networking/msg_sender.h"
#include "networking/wait_mode.h"
#include "orderbook/event_consumer.h"
#include "networking/input_handler.h"
#include <unistd.h>
#include <iostream>
#include <thread>

int main(){
    int socket_fd;
    if(!setup_client(socket_fd)){
        std::cerr << "Error occured!\n";
        return 1;
    }

    EventSink event_sink;
    AtomicBool running = true;
    bool block_mode = read_block_mode_from_env();
    std::thread event_recv_thread(
        run_parser,
        socket_fd,
        std::reference_wrapper(event_sink),
        std::reference_wrapper(running)
    );

    std::thread event_consumer_thread(
        consume_events,
        std::reference_wrapper(event_sink),
        std::reference_wrapper(running),
        block_mode
    );

    std::optional<Message> msg;
    while((msg = read_message()) != std::nullopt){
        run_sender(socket_fd, *msg);
    }

    running = false;

    event_recv_thread.join();
    event_consumer_thread.join();
    close(socket_fd);
}
