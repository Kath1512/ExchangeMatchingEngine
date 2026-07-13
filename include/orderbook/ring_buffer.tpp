#pragma once
#include "ring_buffer.h"


template<typename T, Size Capacity>
constexpr Size RingBuffer<T, Capacity>::next(Size i){
    return (i + 1) & (Capacity - 1); 
}

template<typename T, Size Capacity>
bool RingBuffer<T, Capacity>::push(const T& item){
    Size head = head_.load(std::memory_order_relaxed);
    Size next_head = next(head);
    Size tail = tail_.load(std::memory_order_acquire);
    
    if(next_head == tail){ //full
        return false;
    }
    buffer_[head] = item;
    head_.store(next_head, std::memory_order_release);
    item_available_.release();
    return true;
}

template<typename T, Size Capacity>
bool RingBuffer<T, Capacity>::push(T&& item){
    Size head = head_.load(std::memory_order_relaxed);
    Size next_head = next(head);
    Size tail = tail_.load(std::memory_order_acquire);

    if(next_head == tail){ //full
        return false;
    }
    buffer_[head] = std::move(item);
    head_.store(next_head, std::memory_order_release);
    item_available_.release();
    return true;
}

template<typename T, Size Capacity>
bool RingBuffer<T, Capacity>::pop(T& item){
    Size tail = tail_.load(std::memory_order_relaxed);
    Size head = head_.load(std::memory_order_acquire);
    Size next_tail = next(tail);

    if(head == tail) { // empty 
        return false;
    } 

    item = buffer_[tail];
    tail_.store(next_tail, std::memory_order_release);
    return true;
}

template<typename T, Size Capacity>
bool RingBuffer<T, Capacity>::wait_pop(T& item, std::chrono::milliseconds timeout){
    if(!item_available_.try_acquire_for(timeout)) return false;
    return pop(item);
}

template<typename T, Size Capacity>
bool RingBuffer<T, Capacity>::empty() const { //called by consumer (writing tail)
    Size head = head_.load(std::memory_order_acquire);
    Size tail = tail_.load(std::memory_order_relaxed);
    return head == tail;
}

template<typename T, Size Capacity>
bool RingBuffer<T, Capacity>::full() const { //called by producer (writing head)
    Size head = head_.load(std::memory_order_relaxed);
    Size tail = tail_.load(std::memory_order_acquire);
    return next(head) == tail;
}

template<typename T, Size Capacity>
Size RingBuffer<T, Capacity>::size() const{
    return (head_ - tail_) & (Capacity - 1);
}

template<typename T, Size Capacity>
Size RingBuffer<T, Capacity>::capacity() const{
    return Capacity;
}
