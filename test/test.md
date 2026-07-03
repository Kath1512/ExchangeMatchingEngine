# Order Book Test Suite

## Files

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

