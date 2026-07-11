#pragma once
#include "networking/socket_utils.h"
#include <string>
#include <vector>

inline bool recv_all_string(int fd, std::string& res){
    MessageLen payload_len;
    int got = recv_all(fd, reinterpret_cast<uint8_t*>(&payload_len), sizeof(MessageLen));
    if(got_error(got)) return false;

    std::vector<uint8_t> buf(payload_len);
    got = recv_all(fd, buf.data(), buf.size());
    if(got_error(got)) return false;

    res = std::string(reinterpret_cast<char*>(buf.data()), payload_len);
    return true;
}