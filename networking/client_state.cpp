#include "networking/client_state.h"
#include <cstring>

ClientState::ClientState(int fd_) :
    fd(fd_),
    read_pos(0),
    write_pos(0),
    pending_header({}),
    parse_state(ParseState::ReadingHeader) {}

size_t ClientState::write_limit() const {
    return buf.size() - write_pos;
}

ssize_t ClientState::available_bytes() const {
    return write_pos - read_pos;
}

bool ClientState::buf_is_full() const {
    return write_limit() == 0;
}

BytePointer ClientState::write_p() {
    return buf.data() + write_pos;
}

BytePointer ClientState::read_p() {
    return buf.data() + read_pos;
}

ssize_t ClientState::read(BytePointer dst, size_t sz) {
    if (available_bytes() < static_cast<ssize_t>(sz)) return -1;

    std::memcpy(dst, read_p(), sz);
    read_pos += sz;

    return sz;
}

void ClientState::reset() {
    read_pos = write_pos = 0;
}

void ClientState::pre_check() {
    if (read_pos == write_pos) {
        reset();
        return;
    }

    if (buf_is_full()) {
        size_t remaining = static_cast<size_t>(available_bytes());

        std::memmove(buf.data(), read_p(), remaining);

        read_pos = 0;
        write_pos = remaining;
    }
}
