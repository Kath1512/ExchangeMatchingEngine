#include "networking/transport/tcp.h"
#include "networking/string_parser.h"
#include "networking/string_sender.h"
#include <cstring>

static void server_handshake(int client_fd){
    std::cout << "Waiting for message.....\n";
    std::string response;
    if(!recv_all_string(client_fd, response)) return;

    std::cout << "Response from client: " << response << "\n";
    std::cout << "----------------------------------------------------"
    "-------------------" << "\n";
}

static void client_handshake(int socket_fd, const std::string& name = "Anonymous"){
    std::string greet = std::format("Hello from {}, I'm about to send some order", name);
    if(!send_all_string(socket_fd, greet)) return;

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

    server_handshake(client_fd);
    return true;
}

bool setup_client(int& socket_fd){
    //create socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(got_error(socket_fd)) return false;
    //connect
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

    client_handshake(socket_fd);
    return true;
}
