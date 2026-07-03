#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include "orderbook/order_book.h"
#include "orderbook/ring_buffer.h"
#include "orderbook/constant.h"
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

    CHECK(!sink.empty());

    Event ev;
    CHECK(sink.pop(ev));
    CHECK(std::holds_alternative<OrderAccepted>(ev));
    CHECK(std::get<OrderAccepted>(ev).order_id == 1);

    return true;
}

bool test_cancel_order_emits_canceled()
{
    VectorSink sink;
    TestBook book(sink);
    book.process_message(AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 1, 100, 10, Side::Buy});

    Event ev;
    sink.pop(ev);   // consume the accepted event

    book.process_message(CancelOrder{1});

    CHECK(!sink.empty());
    CHECK(sink.pop(ev));
    CHECK(std::holds_alternative<OrderCanceled>(ev));
    CHECK(std::get<OrderCanceled>(ev).order_id == 1);

    return true;
}

bool test_cancel_unknown_order_emits_rejected()
{
    VectorSink sink;
    TestBook book(sink);
    book.process_message(CancelOrder{999});

    Event ev;
    CHECK(sink.pop(ev));
    CHECK(std::holds_alternative<OrderRejected>(ev));

    return true;
}

bool test_matching_trade_emits_trade_executed()
{
    VectorSink sink;
    TestBook book(sink);
    Event ev;

    // Resting ask → emits OrderAccepted{1}
    book.process_message(AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 1, 100, 10, Side::Sell});
    sink.pop(ev);
    CHECK(std::holds_alternative<OrderAccepted>(ev));
    CHECK(std::get<OrderAccepted>(ev).order_id == 1);

    // Aggressing buy → emits TradeExecuted first, then OrderAccepted{2}
    book.process_message(AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 2, 100, 10, Side::Buy});

    CHECK(sink.pop(ev));
    CHECK(std::holds_alternative<TradeExecuted>(ev));
    const auto& trade = std::get<TradeExecuted>(ev);
    CHECK(trade.price == 100);
    CHECK(trade.quantity == 10);

    CHECK(sink.pop(ev));
    CHECK(std::holds_alternative<OrderAccepted>(ev));
    CHECK(std::get<OrderAccepted>(ev).order_id == 2);

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

    CHECK(sink.pop(ev));
    CHECK(std::holds_alternative<OrderAccepted>(ev));
    CHECK(std::get<OrderAccepted>(ev).order_id == 1);

    CHECK(sink.pop(ev));
    CHECK(std::holds_alternative<OrderAccepted>(ev));
    CHECK(std::get<OrderAccepted>(ev).order_id == 2);

    CHECK(sink.pop(ev));
    CHECK(std::holds_alternative<OrderAccepted>(ev));
    CHECK(std::get<OrderAccepted>(ev).order_id == 3);

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

    CHECK((int)received.size() == 10);
    for (const auto& e : received) {
        CHECK(std::holds_alternative<OrderAccepted>(e));
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
