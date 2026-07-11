#pragma once
#include "networking/socket_utils.h"

using MessageLen = uint64_t;

inline bool send_all_string(int fd, const std::string& msg){
    MessageLen sz = msg.size();
    // std::cout << "Sent: " << sz << "\n";
    int got = send_all(fd, reinterpret_cast<uint8_t*>(&sz), sizeof(MessageLen));

    if(got_error(got)) return false;

    got = send_all(fd, reinterpret_cast<const uint8_t*>(msg.data()), msg.size());

    if(got_error(got)) return false;

    return true;
}