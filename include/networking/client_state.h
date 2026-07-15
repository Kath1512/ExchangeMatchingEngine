#pragma once

#include <array>
#include "networking/constant.h"
#include "networking/protocol.h"

using Byte = uint8_t;
using BytePointer = Byte*;

enum class ParseState {
    ReadingHeader,
    ReadingPayload
};

struct ClientState {
    int fd = -1;
    size_t read_pos = 0;
    size_t write_pos = 0;
    std::array<Byte, STATE_SIZE> buf{};
    OrderMsgHeader pending_header{};
    ParseState parse_state = ParseState::ReadingHeader;

    ClientState() = default;
    explicit ClientState(int fd_);

    size_t write_limit() const;
    ssize_t available_bytes() const;
    bool buf_is_full() const;

    BytePointer write_p();
    BytePointer read_p();

    ssize_t read(BytePointer dst, size_t sz);
    void reset();
    void pre_check();
};
