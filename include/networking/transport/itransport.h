#pragma once
#include <cstdint>
#include <cstddef>
#include <concepts>
#include <unistd.h>

// Transport concept — TCP and UDP must satisfy this
template<typename T>
concept Transport = requires(T t, int fd, int fd2, const uint8_t* sbuf, uint8_t* rbuf, std::size_t len) {
    { t.send(fd, sbuf, len) } -> std::same_as<ssize_t>;
    { t.recv(fd, rbuf, len) } -> std::same_as<ssize_t>;
    { t.setup_server(fd, fd2) } -> std::same_as<bool>;
    { t.setup_client(fd) } -> std::same_as<bool>;
};
