#include<networking/sender.h>
#include<networking/parser.h>
#include<arpa/inet.h>
#include<cstring>
#include<unistd.h>
#include<iostream>
#include<thread>
#include "orderbook/order_book.h"
#include "orderbook/ring_buffer.h"

char message[] = "Hello server, send me some events!";
void test_message(int client_fd){
    std::cout << "Waiting for message.....\n";
    char response[256];
    int got = recv_all(client_fd, reinterpret_cast<uint8_t*>(response), sizeof(message));   
    std::cout << "Got: " << got << "\n";

    std::cout << "Response from client: " << response << "\n";
    std::cout << "----------------------------------------------------"
    "-------------------" << "\n";
}

bool setup_server(int& socket_fd, int& client_fd){
    //create socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(got_error(socket_fd)) return false;
    //bind
    sockaddr_in server_address{};

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(8080);

    int bind_res = bind(socket_fd, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address));
    if(got_error(bind_res)) return false;

    //listen
    int listen_res = listen(socket_fd, 5);
    if(got_error(listen_res)) return false;
    std::cout << "Listening on port 8080\n";
    //accept
    sockaddr_in client_address{};
    socklen_t client_size = sizeof(client_address);
    client_fd = accept(
        socket_fd, reinterpret_cast<sockaddr*>(&client_address),
        &client_size
    );
    if(got_error(client_fd)) return false;


    std::cout << "Client connected\n";
    std::cout << "Family: " << client_address.sin_family << "\n";
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_address.sin_addr, ip, INET_ADDRSTRLEN);
    std::cout << "Address: " << ip  << "\n";

    test_message(client_fd);
    return true;
}

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

    int msg_typ = 0;
    while(msg_typ != -1){
        std::cout << "Enter message type | 1: Add Order | 2: Modify Order | 3: Cancel Order | -1: Quit\n" << std::flush;
        std::cin >> msg_typ;
        switch (msg_typ){
            case 1: {
                OrderId id;
                Price price;
                Quantity quantity;
                int side, tif;
                std::cout << "order_id price quantity side(0=Buy 1=Sell) tif(0=FOK 1=GTC 2=IOC): ";
                std::cin >> id >> price >> quantity >> side >> tif;
                book.process_message(AddOrder{
                    OrderType::Limit,
                    static_cast<TimeInForce>(tif),
                    id, price, quantity,
                    static_cast<Side>(side)
                });
                break;
            }
            case 2: {
                OrderId id;
                Price new_price;
                Quantity new_quantity;
                std::cout << "order_id new_price new_quantity: ";
                std::cin >> id >> new_price >> new_quantity;
                book.process_message(ModifyOrder{id, new_price, new_quantity});
                break;
            }
            case 3: {
                OrderId id;
                std::cout << "order_id: ";
                std::cin >> id;
                book.process_message(CancelOrder{id});
                break;
            }
            case -1:
                break;
            default:
                std::cout << "Unknown type\n";
                break;
        }
    }
    running = false;
    sender_thread.join();
    close(client_fd);
    close(socket_fd);

}