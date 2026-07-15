# Test Suites

The project has four independent test binaries, each testing a different layer.
Run all of them together with `ctest --test-dir build` (see below).

| Suite | Binary | What it exercises | Bypasses |
|---|---|---|---|
| Order book | `test_runner` | `OrderBook` matching logic (343 data-driven cases) | Networking, threading, wire format entirely |
| Ring buffer | `test_ring_buffer` | The SPSC ring buffer container alone | Everything else |
| Event pipeline | `test_event_pipeline` | `OrderBook` + ring buffer threading together | Sockets, wire format, kqueue |
| **E2E pipeline** | `test_e2e_pipeline` | The real kqueue server + real TCP clients over the actual wire protocol | Nothing — this is the only suite that touches sockets |

Only `test_e2e_pipeline` actually drives the client ↔ server boundary. Everything else calls into
`OrderBook`/`RingBuffer` directly in-process, which is faster and fine for logic bugs, but structurally
cannot catch bugs in serialization, the non-blocking parser state machine, multi-client kqueue handling,
or the byte-budget/sequencer fairness logic — that's what the E2E suite is for.

## Order Book Test Suite — Files

| File | Purpose |
|------|---------|
| `test/data_driven.h` | Framework structs, macros, and function declarations |
| `test/data_driven.cpp` | `check_trades`, `check_levels`, `run_order_book_test`, example cases |
| `test.cpp` | Entry point — calls `data_driven_suite()` and `data_driven_summary()` |

---

## How to Build and Run

### Debug (recommended — includes sanitizers)

```bash
bash run_debug.sh
```

This compiles with ASan + UBSan and runs the test binary:

```
clang++ -std=c++20 -Iinclude -Itest -Wall -Wextra -Wpedantic \
        -fsanitize=address,undefined -g \
        src/*.cpp test.cpp -o build/test
```

### Release

```bash
bash run.sh
```

That runs `main.cpp`, not the test suite. To build and run tests in release mode:

```bash
g++ -std=c++20 -Iinclude -Itest src/*.cpp test.cpp -o build/test && ./build/test
```

### Expected output (all passing)

```
[DD] exact_match
[DD] partial_fill
...
── Data-Driven Results ────────────────────────────────────
  Passed: 85 / 85
  All tests passed.
```

`data_driven_summary()` returns `0` on success and `1` on failure — suitable as a `main()` return value for CI.

---

## Test Framework

Defined in `test/data_driven.h` under the `dd` namespace.

### Macros

```cpp
DD_CHECK(expr)       // fails if expr is falsy
DD_CHECK_EQ(a, b)    // fails if a != b
```

On failure:

```
  FAIL  test/data_driven.cpp:38  in [test_name]
        expression that failed
```

### Public API

```cpp
void run_order_book_test(const OrderBookTestCase& tc);  // build book, replay inputs, check results
void data_driven_suite();                               // run all built-in cases
int  data_driven_summary();                             // print totals, return 0/1
```

---

## Adding a Test

Define an `OrderBookTestCase` and pass it to `run_order_book_test` inside `data_driven_suite()`:

```cpp
run_order_book_test({
    .name = "my_scenario",
    .inputs = {
        AddOrder{GTC, 1, 100, 10, Side::Sell},
        AddOrder{GTC, 2, 100, 10, Side::Buy},
    },
    .expected_trades = {
        {.buyer_id=2, .seller_id=1, .price=100, .quantity=10},
    },
    .expected_levels = {},   // empty = book should be empty
});
```

### Input actions

```cpp
AddOrder{tif, id, price, qty, side}         // seq defaults to 1
AddOrder{tif, id, price, qty, side, seq}    // explicit sequence number
CancelOrder{id}
```

### ExpectedTrade fields

```cpp
struct ExpectedTrade {
    OrderId  buyer_id;
    OrderId  seller_id;
    Price    price;     // execution price (always the resting order's price)
    Quantity quantity;
};
```

### ExpectedLevel fields

```cpp
struct ExpectedLevel {
    Side        side;
    Price       price;
    Quantity    total_quantity;
    std::optional<std::size_t> order_count = std::nullopt;  // omit to skip check
};
```

`check_levels` also verifies the total number of bid and ask levels — unexpected resting orders are caught as a count mismatch.

---

## Built-in Example Cases

| Name | What it tests |
|------|--------------|
| `exact_match` | Two orders fully cross, book empty afterward |
| `partial_fill` | Incoming buy smaller than resting sell; residual stays |
| `fifo_same_price` | Two sells at the same price filled in arrival order |
| `multi_level_sweep` | Single buy sweeps two ask price levels |
| `gtc_rests_then_matches` | Order rests until a later opposing order arrives |
| `ioc_partial_fill` | IOC fills what's available; leftover cancelled, doesn't rest |
| `ioc_no_fill` | IOC with nothing to match; cancelled entirely |
| `fok_rejected` | FOK rejected because ask liquidity < order qty |
| `fok_accepted` | FOK fills completely across two ask levels |
| `cancel_order` | Cancelling an order removes its price level |

---

## E2E Pipeline Test (`test/test_e2e_pipeline.cpp`)

Drives the actual `setup_server`/`setup_client` functions from `networking/transport/tcp.cpp` — the same
ones the real `server_kqueue` and `client` binaries use — over real loopback TCP sockets on a dedicated
test port (`8181`, separate from the production default `8080`).

### What it wires up

Same threads as `server/multi_server_kqueue.cpp`, all running in-process inside the test:

```
engine_thread   → consume_messages(book, msg_sink, running, block_mode)
sender_thread   → run_sender(fd_to_state, mutex, event_sink, running, block_mode)   [server-side]
server_thread   → setup_server(msg_sink, fd_to_state, mutex, running, TEST_PORT, on_ready)
```

Plus two real clients, each with `setup_client` + its own `run_parser` thread reading events back:

```
client A: setup_client(fd_a, TEST_PORT) → parser_a: run_parser(fd_a, sink_a, running)
client B: setup_client(fd_b, TEST_PORT) → parser_b: run_parser(fd_b, sink_b, running)
```

The test pops events directly off `sink_a`/`sink_b` with `wait_pop(timeout)` and asserts on their content
— no `consume_events` logging thread involved, since the test itself is the consumer for assertions.

### Scenario: `two_client_crossing_order`

1. Client A sends a `AddOrder` (Sell, price 100, qty 10) — rests, no counterparty yet.
2. **Both** A and B receive a public `BookUpdate` (Sell/100/qty=10) — B sees this even though it hasn't
   sent anything, because it was already connected when the broadcast went out. This surprised the first
   draft of this test; see the `add_order_to_side` note in `todo.md` item 2.
3. A additionally receives its private `OrderRested`.
4. Client B sends a crossing `AddOrder` (Buy, price 100, qty 10) — fully fills A's resting order.
5. Both A and B receive the public `TradeExecuted` then `BookUpdate` (qty now 0), in that order.
6. A (the maker) additionally receives its own private `OrderFilled` — fixed as ERR-016 (`doc/error.md`);
   previously only the taker got a private confirmation.
7. B (the taker) additionally receives its private `OrderFilled`.

### Why teardown uses a `Defer` guard

Every thread is joined and both client fds closed via a `Defer` struct (runs its lambda on scope exit)
constructed right after the threads are spawned — not manual cleanup code at the bottom of the function.
A `CHECK(...)` failure `return`s early; without RAII cleanup, that would destroy still-joinable
`std::thread` objects, which calls `std::terminate`, and if it doesn't die cleanly first, the spin-mode
(`block_mode=false`) consumer threads busy-loop forever since nothing ever sets `running = false`. Hit this
firsthand while developing the test — a leaked process pinned two cores and blocked a later port rebind
until it was manually `kill -9`'d.

Teardown order matters too: stop the server first (`running = false; server_thread.join()`), which closes
both accepted sockets as part of its own graceful shutdown — that's what unblocks the clients' blocking
`recv()` calls with a clean EOF, so `parser_a`/`parser_b` exit on their own. Only after those are joined
does the test close the client-owned fds itself.

### Testability changes this required in production code

`setup_server`/`setup_client` (`include/networking/transport/tcp.h`) gained:
- `port` parameter (was hardcoded to `8080`) — the E2E test runs on `8181` instead
- `AtomicBool& running` — `setup_server`'s accept loop was `while(true)`; now exits when `running` is set
  false, via a bounded (100ms) `kevent()` timeout so it wakes up to check even when idle. This does *not*
  add per-message latency — a pending kqueue event still returns immediately, the timeout only bounds how
  long an *idle* server takes to notice a shutdown request.
- `on_ready` callback — fires once `listen()` succeeds, so a caller knows exactly when `connect()` is safe
  instead of guessing with a sleep. The test still adds a small grace sleep after both clients connect,
  to let the server's kqueue loop register both fds before the test starts sending.
- `SO_REUSEADDR` on the listen socket, so repeated test runs (or a crashed/restarted real server) don't
  fail to bind with "Address already in use" from a lingering `TIME_WAIT`.

Also fixed: `msg_sender.h` and `event_sender.h` each independently defined a helper struct `overloaded` in
the global namespace. No production file had ever included both headers in the same translation unit, so
the duplicate definition never surfaced — the E2E test is the first thing that needs both client-side and
server-side sending together, which triggered the compile error. Extracted to `include/common/overloaded.h`,
included by both.

### Running it standalone

```bash
cmake --build build --target test_e2e_pipeline
./build/test_e2e_pipeline
```

Expected output on success:

```
Listening on port 8181
Client connected
...
Connection closed
Connection closed
[PASS] two_client_crossing_order
1/1 tests passed
```

### Running everything together

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build            # all 4 suites
ctest --test-dir build -R e2e     # just this one
ctest --test-dir build -V         # verbose, shows server/client stdout on failure
```

