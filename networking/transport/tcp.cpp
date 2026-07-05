#include "networking/transport/tcp.h"
#include <cstring>

static void server_handshake(int client_fd){
    char message[] = "Hello server, send me some events!";
    std::cout << "Waiting for message.....\n";
    char response[256];
    int got = recv_all(client_fd, reinterpret_cast<uint8_t*>(response), sizeof(message));
    std::cout << "Got: " << got << "\n";

    std::cout << "Response from client: " << response << "\n";
    std::cout << "----------------------------------------------------"
    "-------------------" << "\n";
}

static void client_handshake(int socket_fd){
    char message[] = "Hello server, send me some events!";
    auto sz = send_all(socket_fd, reinterpret_cast<uint8_t*>(&message), sizeof(message));
    if(got_error(sz)) return;
    std::cout << "Sent: " << sz << "\n";
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
