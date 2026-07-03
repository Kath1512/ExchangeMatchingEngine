#pragma once
#include "ring_buffer.h"
#include "test/data_driven.h"

inline void ring_buffer_suite() {

    // ── 1. Initial state ─────────────────────────────────────────────────────
    dd::current_test = "rb_initial_state";
    {
        RingBuffer<int, 8> rb;
        DD_CHECK(rb.empty());
        DD_CHECK(!rb.full());
        DD_CHECK_EQ(rb.size(), 0u);
        DD_CHECK_EQ(rb.capacity(), 8u);
    }

    // ── 2. Single push and pop round-trip ────────────────────────────────────
    dd::current_test = "rb_single_round_trip";
    {
        RingBuffer<int, 8> rb;
        DD_CHECK(rb.push(42));
        DD_CHECK(!rb.empty());
        DD_CHECK_EQ(rb.size(), 1u);

        int out = 0;
        DD_CHECK(rb.pop(out));
        DD_CHECK_EQ(out, 42);
        DD_CHECK(rb.empty());
        DD_CHECK_EQ(rb.size(), 0u);
    }

    // ── 3. Fill to capacity ──────────────────────────────────────────────────
    dd::current_test = "rb_fill_to_capacity";
    {
        RingBuffer<int, 8> rb;
        for (int i = 0; i < 7; ++i) {   // capacity - 1 items max
            DD_CHECK(rb.push(i));
        }
        DD_CHECK(rb.full());
        DD_CHECK_EQ(rb.size(), 7u);
    }

    // ── 4. Push when full returns false ──────────────────────────────────────
    dd::current_test = "rb_push_when_full";
    {
        RingBuffer<int, 8> rb;
        for (int i = 0; i < 7; ++i) rb.push(i);
        DD_CHECK(!rb.push(99));   // must fail, not corrupt
        DD_CHECK_EQ(rb.size(), 7u);
    }

    // ── 5. Pop when empty returns false ──────────────────────────────────────
    dd::current_test = "rb_pop_when_empty";
    {
        RingBuffer<int, 8> rb;
        int out = 0;
        DD_CHECK(!rb.pop(out));
        DD_CHECK_EQ(out, 0);   // must not be modified
    }

    // ── 6. FIFO ordering ─────────────────────────────────────────────────────
    dd::current_test = "rb_fifo_order";
    {
        RingBuffer<int, 8> rb;
        rb.push(1); rb.push(2); rb.push(3);
        int out = 0;
        rb.pop(out); DD_CHECK_EQ(out, 1);
        rb.pop(out); DD_CHECK_EQ(out, 2);
        rb.pop(out); DD_CHECK_EQ(out, 3);
    }

    // ── 7. Wrap-around ───────────────────────────────────────────────────────
    // Push and pop enough times to force the index to cross the array boundary
    dd::current_test = "rb_wrap_around";
    {
        RingBuffer<int, 8> rb;
        // Advance indices to near the end
        for (int i = 0; i < 6; ++i) { rb.push(i); int tmp; rb.pop(tmp); }
        // Now push and pop across the wrap boundary
        rb.push(100); rb.push(200);
        int out = 0;
        rb.pop(out); DD_CHECK_EQ(out, 100);
        rb.pop(out); DD_CHECK_EQ(out, 200);
        DD_CHECK(rb.empty());
    }

    // ── 8. size() tracks correctly through mixed operations ──────────────────
    dd::current_test = "rb_size_tracking";
    {
        RingBuffer<int, 8> rb;
        rb.push(1); DD_CHECK_EQ(rb.size(), 1u);
        rb.push(2); DD_CHECK_EQ(rb.size(), 2u);
        int tmp;
        rb.pop(tmp); DD_CHECK_EQ(rb.size(), 1u);
        rb.pop(tmp); DD_CHECK_EQ(rb.size(), 0u);
    }
}
