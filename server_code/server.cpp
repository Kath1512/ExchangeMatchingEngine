#include "networking/sender.h"
#include "networking/transport/tcp.h"
#include "networking/input_handler.h"
#include "orderbook/order_book.h"
#include <unistd.h>
#include <iostream>
#include <thread>

int main(){
    int socket_fd, client_fd;
    if(!setup_server(socket_fd, client_fd)){
        std::cerr << "Error occured!\n";
        return 1;
    }

    DefaultSink sink;
    OrderBook<DefaultSink> book(sink);

    AtomicBool running = true;
    std::thread sender_thread(
        run_sender,
        client_fd,
        std::reference_wrapper(sink),
        std::reference_wrapper(running)
    );

    std::optional<Message> msg;
    while((msg = read_message()) != std::nullopt){
        book.process_message(*msg);
    }

    running = false;
    sender_thread.join();
    close(client_fd);
    close(socket_fd);
}
