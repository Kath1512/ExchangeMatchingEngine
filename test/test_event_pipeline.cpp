#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include "orderbook/order_book.h"
#include "common/ring_buffer.h"
#include "engine/constant.h"
#include "orderbook/order_messages.h"
#include "test_sink.h"

#define CHECK(expr)                                          \
    do {                                                     \
        if (!(expr)) {                                       \
            std::cerr << "FAILED: " << #expr                 \
                      << " at " << __FILE__                  \
                      << ":" << __LINE__ << "\n";            \
            return false;                                    \
        }                                                    \
    } while (false)

// ── Tests ─────────────────────────────────────────────────────────────────────

bool test_add_order_emits_accepted()
{
    VectorSink sink;
    TestBook book(sink);
    book.process_message(AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 1, 100, 10, Side::Buy});

    Event ev;
    // BookUpdate fires first (level created), then OrderRested
    CHECK(sink.pop(ev));
    CHECK(is_public<BookUpdate>(ev));
    CHECK(get_public<BookUpdate>(ev).new_total_quantity == 10);

    CHECK(sink.pop(ev));
    CHECK(is_private<OrderRested>(ev));
    CHECK(get_private<OrderRested>(ev).order_id == 1);
    CHECK(get_private<OrderRested>(ev).remaining_quantity == 10);

    return true;
}

bool test_cancel_order_emits_canceled()
{
    VectorSink sink;
    TestBook book(sink);
    book.process_message(AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 1, 100, 10, Side::Buy});

    Event ev;
    sink.pop(ev);   // consume BookUpdate
    sink.pop(ev);   // consume OrderRested

    book.process_message(CancelOrder{1});

    // BookUpdate fires first (level gone, qty=0), then OrderCanceled
    CHECK(sink.pop(ev));
    CHECK(is_public<BookUpdate>(ev));
    CHECK(get_public<BookUpdate>(ev).new_total_quantity == 0);

    CHECK(sink.pop(ev));
    CHECK(is_private<OrderCanceled>(ev));
    CHECK(get_private<OrderCanceled>(ev).order_id == 1);

    return true;
}

bool test_cancel_unknown_order_emits_rejected()
{
    VectorSink sink;
    TestBook book(sink);
    book.process_message(CancelOrder{999});

    Event ev;
    CHECK(sink.pop(ev));
    CHECK(is_private<CancelRejected>(ev));

    return true;
}

bool test_matching_trade_emits_trade_executed()
{
    VectorSink sink;
    TestBook book(sink);
    Event ev;

    // Resting ask → BookUpdate{Ask,100,10}, OrderRested{1}
    book.process_message(AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 1, 100, 10, Side::Sell});
    sink.pop(ev);   // BookUpdate
    sink.pop(ev);   // OrderRested
    CHECK(is_private<OrderRested>(ev));
    CHECK(get_private<OrderRested>(ev).order_id == 1);

    // Aggressing buy (full fill) → TradeExecuted, BookUpdate{Ask,100,0}, OrderFilled{2}
    book.process_message(AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 2, 100, 10, Side::Buy});

    CHECK(sink.pop(ev));
    CHECK(is_public<TradeExecuted>(ev));
    CHECK(get_public<TradeExecuted>(ev).price == 100);
    CHECK(get_public<TradeExecuted>(ev).quantity == 10);

    CHECK(sink.pop(ev));
    CHECK(is_public<BookUpdate>(ev));
    CHECK(get_public<BookUpdate>(ev).new_total_quantity == 0);

    CHECK(sink.pop(ev));
    CHECK(is_private<OrderFilled>(ev));
    CHECK(get_private<OrderFilled>(ev).order_id == 2);

    return true;
}

bool test_event_fifo_order()
{
    VectorSink sink;
    TestBook book(sink);

    book.process_message(AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 1, 100, 10, Side::Buy});
    book.process_message(AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 2, 90,  10, Side::Buy});
    book.process_message(AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 3, 110, 10, Side::Sell});

    Event ev;

    // Each add: BookUpdate then OrderRested
    CHECK(sink.pop(ev)); CHECK(is_public<BookUpdate>(ev));
    CHECK(sink.pop(ev));
    CHECK(is_private<OrderRested>(ev));
    CHECK(get_private<OrderRested>(ev).order_id == 1);

    CHECK(sink.pop(ev)); CHECK(is_public<BookUpdate>(ev));
    CHECK(sink.pop(ev));
    CHECK(is_private<OrderRested>(ev));
    CHECK(get_private<OrderRested>(ev).order_id == 2);

    CHECK(sink.pop(ev)); CHECK(is_public<BookUpdate>(ev));
    CHECK(sink.pop(ev));
    CHECK(is_private<OrderRested>(ev));
    CHECK(get_private<OrderRested>(ev).order_id == 3);

    return true;
}

bool test_concurrent_consumer()
{
    // Concurrent test uses the real ring buffer sink since VectorSink is not thread-safe
    RingBuffer<Event, 128> buffer;
    OrderBook<RingBuffer<Event, 128>> book(buffer);

    AtomicBool running{true};
    std::vector<Event> received;
    std::mutex received_mtx;

    std::thread consumer([&]() {
        Event ev;
        while (running || !buffer.empty()) {
            if (buffer.pop(ev)) {
                std::lock_guard<std::mutex> lock(received_mtx);
                received.push_back(ev);
            }
        }
    });

    for (int i = 1; i <= 10; ++i) {
        book.process_message(AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, (OrderId)i, 100, 10, Side::Buy});
    }

    running = false;
    consumer.join();

    // Each of 10 orders emits BookUpdate + OrderRested = 20 events total
    CHECK((int)received.size() == 20);
    for (const auto& e : received) {
        CHECK(is_public<BookUpdate>(e) || is_private<OrderRested>(e));
    }

    return true;
}

// ── Main ──────────────────────────────────────────────────────────────────────

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

    run("add_order_emits_accepted",          test_add_order_emits_accepted);
    run("cancel_order_emits_canceled",       test_cancel_order_emits_canceled);
    run("cancel_unknown_emits_rejected",     test_cancel_unknown_order_emits_rejected);
    run("matching_trade_emits_executed",     test_matching_trade_emits_trade_executed);
    run("event_fifo_order",                  test_event_fifo_order);
    run("concurrent_consumer",               test_concurrent_consumer);

    std::cout << passed << "/" << total << " tests passed\n";
    return passed == total ? 0 : 1;
}
