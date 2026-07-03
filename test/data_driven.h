#pragma once
#include "orderbook/order_book.h"
#include "orderbook/order.h"
#include "orderbook/types.h"
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "orderbook/order_messages.h"
#include "test_sink.h"

// ════════════════════════════════════════════════════════════════════════════
//  Data-driven test framework for OrderBook
//
//  Workflow:
//    1. Describe each scenario as an OrderBookTestCase (inputs + expectations).
//    2. Call run_order_book_test(tc) — it builds a fresh book, replays every
//       action, then calls check_trades and check_levels automatically.
//    3. Add run_order_book_test(tc) calls inside data_driven_suite().
//    4. Call data_driven_summary() from main() and use its return value.
// ════════════════════════════════════════════════════════════════════════════


// ── Framework state & macros ──────────────────────────────────────────────────

namespace dd {
    inline int  pass_count   = 0;
    inline int  fail_count   = 0;
    inline std::string current_test;

    inline void record(bool ok, const char* expr, const char* file, int line) {
        if (ok) {
            ++pass_count;
        } else {
            ++fail_count;
            std::cerr << "  FAIL  " << file << ":" << line
                      << "  in [" << current_test << "]\n"
                      << "        " << expr << "\n";
        }
    }
} // namespace dd

// Assert expr is truthy.
#define DD_CHECK(expr) \
    dd::record(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

// Assert a == b (uses operator==).
#define DD_CHECK_EQ(a, b) \
    dd::record((a) == (b), #a " == " #b, __FILE__, __LINE__)



// A single test step — same alternatives as Message, so process_message() can
// be called directly on each input without an intermediate visitor.
using TestInput = Message;


// ── Expected outcomes ─────────────────────────────────────────────────────────

struct ExpectedTrade {
    OrderId  buyer_id;
    OrderId  seller_id;
    Price    price;
    Quantity quantity;
};

// Describes one resting price level that should exist in the book at the end.
// order_count is optional — omit it to skip that check.
struct ExpectedLevel {
    Side        side;
    Price       price;
    Quantity    total_quantity;
    std::optional<std::size_t> order_count = std::nullopt;
};


// ── Test case ─────────────────────────────────────────────────────────────────

struct OrderBookTestCase {
    std::string                name;
    std::vector<TestInput>     inputs;
    std::vector<ExpectedTrade> expected_trades;
    // List every level that should be in the book after all inputs are applied.
    // The total number of bid/ask levels is also verified, so any unexpected
    // resting level will be caught as a count mismatch.
    std::vector<ExpectedLevel> expected_levels;
};


// ── Runners ───────────────────────────────────────────────────────────────────

// Verify trade history against expected trades (count + per-trade fields).
void check_trades(const TradeHistory&              actual,
                  const std::vector<ExpectedTrade>& expected);

// Verify final book levels (per-level quantity/count + total level count).
void check_levels(const TestBook&                   book,
                  const std::vector<ExpectedLevel>& expected);

// Build a fresh book, replay all inputs, then call check_trades + check_levels.
void run_order_book_test(const OrderBookTestCase& tc, bool logging);

// Run all built-in example test cases.
void data_driven_suite();
void run_one();

// Print totals. Returns 0 if all passed, 1 otherwise. Use as main() return value.
int data_driven_summary();
