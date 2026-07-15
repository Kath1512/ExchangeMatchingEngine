#include <iostream>
#include <thread>
#include <vector>
#include "common/ring_buffer.h"

#define CHECK(expr)                                      \
    do {                                                 \
        if (!(expr)) {                                   \
            std::cerr << "FAILED: " << #expr             \
                      << " at " << __FILE__              \
                      << ":" << __LINE__ << "\n";        \
            return false;                                \
        }                                                \
    } while (false)

bool test_initial_state()
{
    RingBuffer<int, 8> q;

    CHECK(q.empty());
    CHECK(!q.full());
    CHECK(q.size() == 0);
    CHECK(q.capacity() == 8);

    return true;
}

bool test_push_pop_order()
{
    RingBuffer<int, 8> q;

    CHECK(q.push(1));
    CHECK(q.push(2));
    CHECK(q.push(3));

    int x;

    CHECK(q.pop(x));
    CHECK(x == 1);

    CHECK(q.pop(x));
    CHECK(x == 2);

    CHECK(q.pop(x));
    CHECK(x == 3);

    CHECK(!q.pop(x));

    return true;
}

bool test_fill_to_capacity()
{
    RingBuffer<int, 4> q;  // holds 3 items max (one slot reserved)

    CHECK(q.push(1));
    CHECK(q.push(2));
    CHECK(q.push(3));
    CHECK(q.full());
    CHECK(q.size() == 3);

    return true;
}

bool test_push_when_full()
{
    RingBuffer<int, 4> q;

    q.push(1); q.push(2); q.push(3);

    CHECK(!q.push(99));    // must return false, not corrupt state
    CHECK(q.size() == 3);

    int x;
    q.pop(x); CHECK(x == 1);   // data must still be intact
    q.pop(x); CHECK(x == 2);
    q.pop(x); CHECK(x == 3);

    return true;
}

bool test_pop_when_empty()
{
    RingBuffer<int, 8> q;

    int x = 42;
    CHECK(!q.pop(x));
    CHECK(x == 42);   // must not modify out param

    return true;
}

bool test_wraparound()
{
    RingBuffer<int, 4> q;

    int x;

    CHECK(q.push(1));
    CHECK(q.push(2));
    CHECK(q.push(3));
    CHECK(!q.push(4));  // full — one slot reserved

    CHECK(q.pop(x));
    CHECK(x == 1);

    CHECK(q.push(4));   // now there's space, index wraps around

    CHECK(q.pop(x));
    CHECK(x == 2);

    CHECK(q.pop(x));
    CHECK(x == 3);

    CHECK(q.pop(x));
    CHECK(x == 4);

    CHECK(!q.pop(x));

    return true;
}

bool test_size_tracking()
{
    RingBuffer<int, 8> q;

    q.push(10); CHECK(q.size() == 1);
    q.push(20); CHECK(q.size() == 2);
    q.push(30); CHECK(q.size() == 3);

    int x;
    q.pop(x); CHECK(q.size() == 2);
    q.pop(x); CHECK(q.size() == 1);
    q.pop(x); CHECK(q.size() == 0);
    CHECK(q.empty());

    return true;
}

bool test_size_after_wraparound()
{
    RingBuffer<int, 8> q;

    // Advance indices to near the boundary
    int x;
    for (int i = 0; i < 6; ++i) {
        q.push(i);
        q.pop(x);
    }

    // Now push 3 items across the wrap boundary and verify size
    q.push(10);
    q.push(20);
    q.push(30);

    CHECK(q.size() == 3);

    q.pop(x); CHECK(x == 10); CHECK(q.size() == 2);
    q.pop(x); CHECK(x == 20); CHECK(q.size() == 1);
    q.pop(x); CHECK(x == 30); CHECK(q.size() == 0);

    return true;
}

bool test_concurrent_producer_consumer()
{
    RingBuffer<int, 64> q;
    const int N = 1000;
    std::vector<int> received;
    received.reserve(N);

    std::thread producer([&]() {
        for (int i = 0; i < N; ++i) {
            while (!q.push(i));   // spin until space available
        }
    });

    std::thread consumer([&]() {
        int x;
        while ((int)received.size() < N) {
            if (q.pop(x)) {
                received.push_back(x);
            }
        }
    });

    producer.join();
    consumer.join();

    CHECK((int)received.size() == N);
    for (int i = 0; i < N; ++i) {
        CHECK(received[i] == i);   // FIFO order preserved
    }

    return true;
}

int main()
{
    int passed = 0;
    int total  = 0;

    auto run = [&](const char* name, bool (*test)()) {
        ++total;
        if (test()) {
            ++passed;
            std::cout << "[PASS] " << name << "\n";
        } else {
            std::cout << "[FAIL] " << name << "\n";
        }
    };

    run("initial_state",    test_initial_state);
    run("push_pop_order",   test_push_pop_order);
    run("fill_to_capacity", test_fill_to_capacity);
    run("push_when_full",   test_push_when_full);
    run("pop_when_empty",   test_pop_when_empty);
    run("wraparound",       test_wraparound);
    run("size_tracking",             test_size_tracking);
    run("size_after_wraparound",     test_size_after_wraparound);
    run("concurrent_producer_consumer", test_concurrent_producer_consumer);

    std::cout << passed << "/" << total << " tests passed\n";

    return passed == total ? 0 : 1;
}
