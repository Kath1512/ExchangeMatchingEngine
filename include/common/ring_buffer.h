#pragma once
#include<atomic>
#include<array>
#include<semaphore>
#include<chrono>
#include "common/types.h"

template<typename T, Size Capacity>
class RingBuffer{
private:
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
    std::array<T, Capacity> buffer_;
    alignas(64) std::atomic<Size> head_{0};
    alignas(64) std::atomic<Size> tail_{0};
    std::counting_semaphore<> item_available_{0};

    static constexpr Size next(Size i);
public:
    RingBuffer() = default;

    bool push(const T& item);
    bool push(T&& item);

    bool pop(T& item);

    // Blocks (no CPU spin) until an item is available or the timeout
    // elapses. Only safe with a single consumer thread per buffer.
    bool wait_pop(T& item, std::chrono::milliseconds timeout);

    bool empty() const;
    bool full() const;

    Size size() const;
    Size capacity() const;
};

#include "ring_buffer.tpp"