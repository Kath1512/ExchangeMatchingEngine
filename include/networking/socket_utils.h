#pragma once
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <cstdint>

inline ssize_t send_all(int fd, const uint8_t* buf, size_t len){
    ssize_t slen = static_cast<ssize_t>(len);
    ssize_t sent = 0;
    while(sent < slen){
        ssize_t cur_sent = send(fd, buf + sent, slen - sent, 0);

        if(cur_sent == -1){
            std::cerr << "Send error: " << strerror(errno) << "\n";
            return -1;
        }

        sent += cur_sent;
    }

    return sent;
}

inline ssize_t recv_all(int fd, uint8_t* buf, size_t len){
    ssize_t slen = static_cast<ssize_t>(len);
    ssize_t recved = 0;
    while(recved < len){
        ssize_t cur_recved = recv(fd, buf + slen, slen - recved, 0);

        if(cur_recved == -1){
            std::cerr << "Receive error: " << strerror(errno) << "\n";
            return -1;
        }
        if(cur_recved  == 0){
            std::cerr << "Connection closed";
            return -1;
        }
        
        recved += cur_recved;
    }

    return recved;
}


