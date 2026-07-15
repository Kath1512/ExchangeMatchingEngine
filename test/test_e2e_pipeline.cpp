// End-to-end test: drives the real kqueue server and two real TCP clients
// through the actual wire protocol (no in-process shortcuts). Verifies a
// resting order and a crossing order produce the right private/public
// events, in the right order, on the right sockets.

#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>
#include <unistd.h>

#include "networking/transport/tcp.h"
#include "networking/msg_parser.h"
#include "networking/msg_sender.h"
#include "networking/event_parser.h"
#include "networking/event_sender.h"
#include "engine/message_consumer.h"

#define CHECK(expr)                                      \
    do {                                                 \
        if (!(expr)) {                                   \
            std::cerr << "FAILED: " << #expr             \
                      << " at " << __FILE__              \
                      << ":" << __LINE__ << "\n";        \
            return false;                                \
        }                                                \
    } while (false)

constexpr int TEST_PORT = 8181;
constexpr auto EVENT_TIMEOUT = std::chrono::milliseconds(1000);
constexpr auto SERVER_READY_TIMEOUT = std::chrono::seconds(2);

//RAII
struct Defer {
    std::function<void()> fn;
    ~Defer() { if (fn) fn(); }
};

template<typename T>
static const T* as_private(const Event& ev) {
    auto* routed = std::get_if<RoutedEvent>(&ev);
    if (!routed) return nullptr;
    return std::get_if<T>(&routed->event);
}

template<typename T>
static const T* as_public(const Event& ev) {
    auto* pub = std::get_if<PublicEvent>(&ev);
    if (!pub) return nullptr;
    return std::get_if<T>(pub);
}

static std::optional<Event> wait_pop(EventSink& sink, std::chrono::milliseconds timeout) {
    Event ev;
    if (sink.wait_pop(ev, timeout)) return ev;
    return std::nullopt;
}

bool test_two_client_crossing_order() {
    EventSink server_event_sink;
    MessageSink msg_sink;
    DefaultBook book(server_event_sink);
    ClientStateList fd_to_state;
    std::mutex fd_to_state_mutex;
    AtomicBool running = true;

    int fd_a = -1, fd_b = -1;
    std::optional<std::thread> parser_a, parser_b;

    std::thread engine_thread(consume_messages, std::ref(book), std::ref(msg_sink), std::ref(running), false);

    //overload run_sender, thread can't
    std::thread sender_thread([&]{
        run_sender(fd_to_state, fd_to_state_mutex, server_event_sink, running, false);
    });
    std::promise<void> server_ready;
    std::thread server_thread([&]{
        setup_server(msg_sink, fd_to_state, fd_to_state_mutex, running, TEST_PORT,
            [&]{ server_ready.set_value(); });
    });

    // Order matters: stop the server first so it closes both accepted
    // sockets, which unblocks the clients' recv() calls with a clean EOF.
    // Only after the parser threads have exited do we close the client fds
    // ourselves — closing a fd out from under a thread still blocked in
    // recv() on it is not something to rely on.
    Defer cleanup{[&]{
        running = false;
        server_thread.join();
        if (parser_a) parser_a->join();
        if (parser_b) parser_b->join();
        engine_thread.join();
        sender_thread.join();
        if (fd_a >= 0) close(fd_a);
        if (fd_b >= 0) close(fd_b);
    }};

    // If setup_server never gets to listen() (e.g. bind failure), on_ready
    // is never called — bound-wait instead of blocking forever.
    CHECK(server_ready.get_future().wait_for(SERVER_READY_TIMEOUT) == std::future_status::ready);

    // ── Two clients ──────────────────────────────────────────────────────
    CHECK(setup_client(fd_a, TEST_PORT));
    CHECK(setup_client(fd_b, TEST_PORT));

    EventSink sink_a, sink_b;
    parser_a.emplace(run_parser, fd_a, std::ref(sink_a), std::ref(running));
    parser_b.emplace(run_parser, fd_b, std::ref(sink_b), std::ref(running));

    // Give the server's accept loop a moment to register both connections
    // with kqueue before we start sending.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // ── Client A rests a sell order — no counterparty yet ──────────────────
    run_sender(fd_a, Message{AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 1, 100, 10, Side::Sell}});

    // Resting a limit order broadcasts a public BookUpdate for the new
    // depth (order_book.h: add_order_to_side) *before* the owner's private
    // confirmation — and since client B is already connected, B sees this
    // broadcast too, even though it hasn't sent anything yet.
    for (EventSink* sink : {&sink_a, &sink_b}) {
        auto ev = wait_pop(*sink, EVENT_TIMEOUT);
        CHECK(ev.has_value());
        auto* book_update = as_public<BookUpdate>(*ev);
        CHECK(book_update != nullptr);
        CHECK(book_update->side == Side::Sell);
        CHECK(book_update->price == 100);
        CHECK(book_update->new_total_quantity == 10);
    }

    auto ev_rested = wait_pop(sink_a, EVENT_TIMEOUT);
    CHECK(ev_rested.has_value());
    auto* rested = as_private<OrderRested>(*ev_rested);
    CHECK(rested != nullptr);
    CHECK(rested->client_order_id == 1);
    CHECK(rested->remaining_quantity == 10);

    // ── Client B crosses it with a buy at the same price ────────────────────
    run_sender(fd_b, Message{AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 2, 100, 10, Side::Buy}});

    // Both clients are subscribed to the public feed, so both see the trade
    // and the resulting book update (level now empty), in that order.
    for (EventSink* sink : {&sink_a, &sink_b}) {
        auto trade_ev = wait_pop(*sink, EVENT_TIMEOUT);
        CHECK(trade_ev.has_value());
        auto* trade = as_public<TradeExecuted>(*trade_ev);
        CHECK(trade != nullptr);
        CHECK(trade->price == 100);
        CHECK(trade->quantity == 10);
        CHECK(trade->aggressive_side == Side::Buy);

        auto book_ev = wait_pop(*sink, EVENT_TIMEOUT);
        CHECK(book_ev.has_value());
        auto* book_update = as_public<BookUpdate>(*book_ev);
        CHECK(book_update != nullptr);
        CHECK(book_update->side == Side::Sell);
        CHECK(book_update->new_total_quantity == 0);
    }

    // Maker (client A) gets its own private fill confirmation too — ERR-016
    // fix: previously only the taker got one.
    auto ev_maker_filled = wait_pop(sink_a, EVENT_TIMEOUT);
    CHECK(ev_maker_filled.has_value());
    auto* maker_filled = as_private<OrderFilled>(*ev_maker_filled);
    CHECK(maker_filled != nullptr);
    CHECK(maker_filled->client_order_id == 1);

    // Taker (client B) gets its own private fill confirmation.
    auto ev_filled = wait_pop(sink_b, EVENT_TIMEOUT);
    CHECK(ev_filled.has_value());
    auto* filled = as_private<OrderFilled>(*ev_filled);
    CHECK(filled != nullptr);
    CHECK(filled->client_order_id == 2);

    return true;
}

int main() {
    int passed = 0;
    int total = 0;

    auto run = [&](const char* name, bool (*test)()) {
        ++total;
        if (test()) {
            ++passed;
            std::cout << "[PASS] " << name << "\n";
        } else {
            std::cout << "[FAIL] " << name << "\n";
        }
    };

    run("two_client_crossing_order", test_two_client_crossing_order);

    std::cout << passed << "/" << total << " tests passed\n";
    return passed == total ? 0 : 1;
}
