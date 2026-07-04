#include<networking/sender.h>
#include<networking/parser.h>
#include<arpa/inet.h>
#include<cstring>
#include<unistd.h>
#include<iostream>
#include<thread>
#include<chrono>
#include"orderbook/event_consumer.h"

void test_message(int socket_fd){
    char message[] = "Hello server, send me some events!";
    auto sz = send_all(socket_fd, reinterpret_cast<uint8_t*>(&message), sizeof(message));
    if(got_error(sz)) return;
    std::cout << "Sent: " << sz << "\n";
    std::cout << "----------------------------------------------------"
    "-------------------" << "\n";

}
bool setup_client(int& socket_fd){
    //create socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(got_error(socket_fd)) return false;
    //bind
    sockaddr_in server_address{};

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr.s_addr);

    int connect_res = connect(
        socket_fd, 
        reinterpret_cast<sockaddr*>(&server_address), 
        sizeof(server_address)
    );
    if(got_error(connect_res)) return false;

    test_message(socket_fd);
    return true;
}

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
