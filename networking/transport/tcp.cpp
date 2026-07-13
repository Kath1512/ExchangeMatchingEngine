#include "networking/transport/tcp.h"
#include "networking/string_parser.h"
#include "networking/string_sender.h"
#include <cstring>


static void print_detail(const sockaddr_in& client_address){
    std::cout << "Client connected\n";
    std::cout << "Family: " << client_address.sin_family << "\n";
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_address.sin_addr, ip, INET_ADDRSTRLEN);
    std::cout << "Address: " << ip  << "\n";
}
static bool get_client(int socket_fd, int& client_fd, sockaddr_in& client_address){
    socklen_t client_size = sizeof(client_address);
    client_fd = accept(
        socket_fd,
        reinterpret_cast<sockaddr*>(&client_address),
        &client_size
    );
    if(client_fd == -1){
        std::cout << "Error: " << strerror(errno) << "\n";
        return false;
    } 
    return true;
}

static bool set_non_blocking(int fd){
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1){
        perror("fcntl(F_GETFL)");
        return false;
    }
    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1){
        perror("fcntl(F_SETFL)");
        return false;
    }
    return true;
}


// static void client_handshake(int socket_fd, const std::string& name = "Anonymous"){
//     // std::string greet = std::format("Hello from {}, I'm about to send some order", name);
//     // if(!send_all_string(socket_fd, greet)) return;

//     std::cout << "----------------------------------------------------"
//     "-------------------" << "\n";
// }

bool setup_server(MessageSink& sink, ClientStateList& fd_to_state, std::mutex& state_mutex){
    //create socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
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
    set_non_blocking(socket_fd);

    int kq = kqueue();
    struct kevent change; //this should be registered to listening socket

    EV_SET( //fill kevent's fields
        &change,
        socket_fd,
        EVFILT_READ,
        EV_ADD,
        0,
        0,
        nullptr
    );

    //register
    kevent(
        kq,
        &change,
        1,
        nullptr,
        0,
        nullptr
    );

    struct kevent events[64];

    while(true){
        int nev = kevent(
            kq,
            nullptr,
            0,
            events,
            64,
            nullptr
        );
        
        //handle each 
        for(int i = 0; i < nev; i++){
            //if ident is listening socket -> register new event;
            if(events[i].ident == static_cast<unsigned long>(socket_fd)){
                int client_fd;
                sockaddr_in client_address{};

                if(!get_client(socket_fd, client_fd, client_address)) continue;
                print_detail(client_address);

                //register non-blocking io
                set_non_blocking(client_fd);
                {
                    std::lock_guard<std::mutex> lock(state_mutex);
                    fd_to_state.insert({client_fd, ClientState(client_fd)});
                }

                struct kevent ev;
                EV_SET(
                    &ev,
                    client_fd,
                    EVFILT_READ,
                    EV_ADD,
                    0,
                    0,
                    nullptr
                );
                kevent(
                    kq,
                    &ev,
                    1,
                    nullptr,
                    0,
                    nullptr
                );
                std::cout << std::format("Waiting for order from client {}.....\n", client_fd);
            }
            else{
                int client_fd = static_cast<int>(events[i].ident);
                auto& state = fd_to_state.at(client_fd);
                state.pre_check();
                auto got = recv_nb_til_limit(client_fd, state.write_p(), state.write_limit());
                if(got == -1 || got == -2){
                    close(client_fd);
                    std::lock_guard<std::mutex> lock(state_mutex);
                    fd_to_state.erase(client_fd);
                    continue;
                }
                state.write_pos += got;
                parse_ready_client(state, sink, client_fd);
            }
        }
    }
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

    // client_handshake(socket_fd);
    return true;
}
