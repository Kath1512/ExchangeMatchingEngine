#include "data_driven.h"
#include <algorithm>

// ── check_trades ─────────────────────────────────────────────────────────────

void check_trades(const TradeHistory&              actual,
                  const std::vector<ExpectedTrade>& expected) {
    DD_CHECK_EQ((int)actual.size(), (int)expected.size());

    const int n = static_cast<int>(std::min(actual.size(), expected.size()));
    for (int i = 0; i < n; ++i) {
        const Trade&         t  = actual[i];
        const ExpectedTrade& e  = expected[i];
        DD_CHECK_EQ(t.get_buyer().get_order_id(),  e.buyer_id);
        DD_CHECK_EQ(t.get_seller().get_order_id(), e.seller_id);
        DD_CHECK_EQ(t.get_execution_price(),        e.price);
        DD_CHECK_EQ(t.get_execution_quantity(),     e.quantity);
    }
}

// ── check_levels ─────────────────────────────────────────────────────────────

void check_levels(const OrderBook&                  book,
                  const std::vector<ExpectedLevel>& expected) {
    // Tally how many bid/ask levels we expect, then verify the totals.
    // Any unexpected resting level is caught here as a count mismatch.
    std::size_t exp_bids = 0, exp_asks = 0;
    for (const auto& lv : expected) {
        if (lv.side == Side::Buy) ++exp_bids;
        else                      ++exp_asks;
    }
    DD_CHECK_EQ(book.bid_level_count(), exp_bids);
    DD_CHECK_EQ(book.ask_level_count(), exp_asks);

    // Verify each expected level exists with the right quantity (and optional count).
    for (const auto& lv : expected) {
        DD_CHECK(book.has_level(lv.side, lv.price));
        DD_CHECK_EQ(book.level_quantity(lv.side, lv.price), lv.total_quantity);
        if (lv.order_count.has_value()) {
            DD_CHECK_EQ(book.level_order_count(lv.side, lv.price), lv.order_count.value());
        }
    }
}

// ── run_order_book_test ───────────────────────────────────────────────────────

void run_order_book_test(const OrderBookTestCase& tc) {
    dd::current_test = tc.name;
    std::cout << "[DD] " << tc.name << "\n";

    OrderBook book;
    for (const TestInput& input : tc.inputs) {
        std::visit([&book](const auto& action) {
            using T = std::decay_t<decltype(action)>;
            if constexpr (std::is_same_v<T, AddOrder>) {
                book.add_order(Order(action.tif, action.id,
                                     action.price, action.qty,
                                     action.side, action.seq));
            } else {
                book.cancel_order(action.id);
            }
        }, input);
    }

    check_trades(book.get_trades(), tc.expected_trades);
    check_levels(book, tc.expected_levels);
}

// ── Example test cases ────────────────────────────────────────────────────────

// Aliases for readability inside test case definitions.
static constexpr TimeInForce GTC = TimeInForce::GoodTillCancel;
static constexpr TimeInForce IOC = TimeInForce::ImmediateOrCancel;
static constexpr TimeInForce FOK = TimeInForce::FillOrKill;

// ── 1. Exact match ────────────────────────────────────────────────────────────
// Two orders of equal size crossing at the same price.
// Both are fully consumed; book is empty afterward.
static const OrderBookTestCase tc_exact_match {
    .name = "exact_match",
    .inputs = {
        AddOrder{GTC, 1, 100, 10, Side::Sell},
        AddOrder{GTC, 2, 100, 10, Side::Buy},
    },
    .expected_trades = {
        {.buyer_id=2, .seller_id=1, .price=100, .quantity=10},
    },
    .expected_levels = {},
};

// ── 2. Partial fill ───────────────────────────────────────────────────────────
// The incoming buy is smaller than the resting sell.
// The sell level survives with the residual quantity.
static const OrderBookTestCase tc_partial_fill {
    .name = "partial_fill",
    .inputs = {
        AddOrder{GTC, 1, 100, 20, Side::Sell},
        AddOrder{GTC, 2, 100,  8, Side::Buy},
    },
    .expected_trades = {
        {.buyer_id=2, .seller_id=1, .price=100, .quantity=8},
    },
    .expected_levels = {
        {.side=Side::Sell, .price=100, .total_quantity=12, .order_count=1},
    },
};

// ── 3. FIFO same-price ────────────────────────────────────────────────────────
// Two sell orders at the same price. A buy that sweeps them must fill the
// earlier-arriving sell first, then the second for the remainder.
static const OrderBookTestCase tc_fifo_same_price {
    .name = "fifo_same_price",
    .inputs = {
        AddOrder{GTC, 1, 100, 10, Side::Sell},   // arrives first
        AddOrder{GTC, 2, 100, 15, Side::Sell},   // arrives second
        AddOrder{GTC, 3, 100, 12, Side::Buy},    // fills 10 from #1, then 2 from #2
    },
    .expected_trades = {
        {.buyer_id=3, .seller_id=1, .price=100, .quantity=10},
        {.buyer_id=3, .seller_id=2, .price=100, .quantity=2},
    },
    .expected_levels = {
        {.side=Side::Sell, .price=100, .total_quantity=13, .order_count=1},
    },
};

// ── 4. Multi-level sweep ──────────────────────────────────────────────────────
// A single buy order sweeps two distinct ask price levels entirely.
static const OrderBookTestCase tc_multi_level_sweep {
    .name = "multi_level_sweep",
    .inputs = {
        AddOrder{GTC, 1, 100, 10, Side::Sell},
        AddOrder{GTC, 2, 110,  5, Side::Sell},
        AddOrder{GTC, 3, 110, 15, Side::Buy},    // crosses both levels
    },
    .expected_trades = {
        {.buyer_id=3, .seller_id=1, .price=100, .quantity=10},
        {.buyer_id=3, .seller_id=2, .price=110, .quantity=5},
    },
    .expected_levels = {},
};

// ── 5. GTC rests then matches ─────────────────────────────────────────────────
// An order that cannot cross rests until a later opposing order arrives.
static const OrderBookTestCase tc_gtc_rests_then_matches {
    .name = "gtc_rests_then_matches",
    .inputs = {
        AddOrder{GTC, 1, 100, 10, Side::Sell},   // rests
        AddOrder{GTC, 2,  90,  5, Side::Buy},    // 90 < 100: does not cross, rests
        AddOrder{GTC, 3,  90,  5, Side::Sell},   // crosses bid #2 at 90
    },
    .expected_trades = {
        {.buyer_id=2, .seller_id=3, .price=90, .quantity=5},
    },
    .expected_levels = {
        {.side=Side::Sell, .price=100, .total_quantity=10, .order_count=1},
    },
};

// ── 6. IOC partial fill ───────────────────────────────────────────────────────
// IOC fills whatever is immediately available; the unfilled remainder is
// cancelled and must not appear as a resting bid.
static const OrderBookTestCase tc_ioc_partial_fill {
    .name = "ioc_partial_fill",
    .inputs = {
        AddOrder{GTC, 1, 100,  5, Side::Sell},
        AddOrder{IOC, 2, 100, 10, Side::Buy},    // fills 5, cancels leftover 5
    },
    .expected_trades = {
        {.buyer_id=2, .seller_id=1, .price=100, .quantity=5},
    },
    .expected_levels = {},
};

// ── 7. IOC no fill ────────────────────────────────────────────────────────────
// When no matching liquidity exists the IOC is cancelled entirely.
static const OrderBookTestCase tc_ioc_no_fill {
    .name = "ioc_no_fill",
    .inputs = {
        AddOrder{IOC, 1, 100, 10, Side::Buy},    // nothing to match against
    },
    .expected_trades = {},
    .expected_levels = {},
};

// ── 8. FOK rejected ───────────────────────────────────────────────────────────
// FOK is rejected when available ask liquidity is less than the order qty.
// The resting ask is untouched; no trade occurs.
static const OrderBookTestCase tc_fok_rejected {
    .name = "fok_rejected",
    .inputs = {
        AddOrder{GTC, 1, 100, 10, Side::Sell},
        AddOrder{FOK, 2, 100, 15, Side::Buy},    // needs 15, only 10 available → rejected
    },
    .expected_trades = {},
    .expected_levels = {
        {.side=Side::Sell, .price=100, .total_quantity=10, .order_count=1},
    },
};

// ── 9. FOK accepted ───────────────────────────────────────────────────────────
// FOK succeeds when ask liquidity across levels exactly covers the qty.
// Both ask levels are swept; book is empty afterward.
static const OrderBookTestCase tc_fok_accepted {
    .name = "fok_accepted",
    .inputs = {
        AddOrder{GTC, 1, 100, 10, Side::Sell},
        AddOrder{GTC, 2, 105,  5, Side::Sell},
        AddOrder{FOK, 3, 105, 15, Side::Buy},    // 10@100 + 5@105 = 15 → fills
    },
    .expected_trades = {
        {.buyer_id=3, .seller_id=1, .price=100, .quantity=10},
        {.buyer_id=3, .seller_id=2, .price=105, .quantity=5},
    },
    .expected_levels = {},
};

// ── 10. Sell sweep bids ───────────────────────────────────────────────────────
// A sell order sweeps two bid levels in descending price order (highest bid
// first). The lower bid is only partially consumed; its residual stays.
static const OrderBookTestCase tc_sell_sweep_bids {
    .name = "sell_sweep_bids",
    .inputs = {
        AddOrder{GTC, 1, 100, 15, Side::Buy},
        AddOrder{GTC, 2,  90, 10, Side::Buy},
        AddOrder{GTC, 3,  85, 20, Side::Sell},   // crosses both bids; 15+5=20
    },
    .expected_trades = {
        {.buyer_id=1, .seller_id=3, .price=100, .quantity=15},
        {.buyer_id=2, .seller_id=3, .price=90,  .quantity=5},
    },
    .expected_levels = {
        {.side=Side::Buy, .price=90, .total_quantity=5, .order_count=1},
    },
};

// ── 11. GTC partial fill rests ────────────────────────────────────────────────
// A GTC buy larger than the resting ask fills what's available, then the
// unfilled remainder rests on the bid side.
static const OrderBookTestCase tc_gtc_partial_fill_rests {
    .name = "gtc_partial_fill_rests",
    .inputs = {
        AddOrder{GTC, 1, 100,  5, Side::Sell},
        AddOrder{GTC, 2, 100, 12, Side::Buy},    // fills 5, leftover 7 rests as bid
    },
    .expected_trades = {
        {.buyer_id=2, .seller_id=1, .price=100, .quantity=5},
    },
    .expected_levels = {
        {.side=Side::Buy, .price=100, .total_quantity=7, .order_count=1},
    },
};

// ── 12. IOC sell ──────────────────────────────────────────────────────────────
// IOC sell fills against a resting bid; unfilled remainder is cancelled.
static const OrderBookTestCase tc_ioc_sell {
    .name = "ioc_sell",
    .inputs = {
        AddOrder{GTC, 1, 100,  8, Side::Buy},
        AddOrder{IOC, 2, 100, 15, Side::Sell},   // fills 8, cancels leftover 7
    },
    .expected_trades = {
        {.buyer_id=1, .seller_id=2, .price=100, .quantity=8},
    },
    .expected_levels = {},
};

// ── 13. FOK sell rejected ─────────────────────────────────────────────────────
// FOK sell rejected because total bid liquidity is less than the order qty.
static const OrderBookTestCase tc_fok_sell_rejected {
    .name = "fok_sell_rejected",
    .inputs = {
        AddOrder{GTC, 1, 100, 10, Side::Buy},
        AddOrder{FOK, 2, 100, 15, Side::Sell},   // needs 15, only 10 available
    },
    .expected_trades = {},
    .expected_levels = {
        {.side=Side::Buy, .price=100, .total_quantity=10, .order_count=1},
    },
};

// ── 14. FOK sell accepted ─────────────────────────────────────────────────────
// FOK sell sweeps two bid levels when total bid qty exactly covers it.
static const OrderBookTestCase tc_fok_sell_accepted {
    .name = "fok_sell_accepted",
    .inputs = {
        AddOrder{GTC, 1, 100, 10, Side::Buy},
        AddOrder{GTC, 2,  90,  5, Side::Buy},
        AddOrder{FOK, 3,  90, 15, Side::Sell},   // 10+5=15 → fills
    },
    .expected_trades = {
        {.buyer_id=1, .seller_id=3, .price=100, .quantity=10},
        {.buyer_id=2, .seller_id=3, .price=90,  .quantity=5},
    },
    .expected_levels = {},
};

// ── 15. Cancel then rematch ───────────────────────────────────────────────────
// After cancelling a resting bid, a new buy order correctly fills against
// the existing ask — verifying the book is consistent post-cancel.
static const OrderBookTestCase tc_cancel_then_rematch {
    .name = "cancel_then_rematch",
    .inputs = {
        AddOrder{GTC, 1, 100, 10, Side::Sell},
        AddOrder{GTC, 2,  90,  5, Side::Buy},    // rests; does not cross ask
        CancelOrder{2},
        AddOrder{GTC, 3, 100, 10, Side::Buy},    // crosses ask 1
    },
    .expected_trades = {
        {.buyer_id=3, .seller_id=1, .price=100, .quantity=10},
    },
    .expected_levels = {},
};

// ── 16. Multiple resting levels both sides ────────────────────────────────────
// Non-crossing orders on both sides all rest; no trades generated.
// Verifies bid/ask level counts and quantities simultaneously.
static const OrderBookTestCase tc_multiple_resting_levels {
    .name = "multiple_resting_levels",
    .inputs = {
        AddOrder{GTC, 1, 110, 10, Side::Sell},
        AddOrder{GTC, 2, 120,  5, Side::Sell},
        AddOrder{GTC, 3,  90,  8, Side::Buy},
        AddOrder{GTC, 4,  80, 12, Side::Buy},
    },
    .expected_trades = {},
    .expected_levels = {
        {.side=Side::Sell, .price=110, .total_quantity=10, .order_count=1},
        {.side=Side::Sell, .price=120, .total_quantity=5,  .order_count=1},
        {.side=Side::Buy,  .price=90,  .total_quantity=8,  .order_count=1},
        {.side=Side::Buy,  .price=80,  .total_quantity=12, .order_count=1},
    },
};

// ── 17. Cancel order ──────────────────────────────────────────────────────────
// Cancelling one of two orders at the same level adjusts quantity and count.
// Cancelling the sole order at a level removes that level entirely.
static const OrderBookTestCase tc_cancel_order {
    .name = "cancel_order",
    .inputs = {
        AddOrder{GTC, 1, 100, 10, Side::Sell},   // first order at @100
        AddOrder{GTC, 2, 100,  5, Side::Sell},   // second order at @100
        AddOrder{GTC, 3,  90, 20, Side::Buy},    // sole bid
        CancelOrder{1},                           // @100 level: qty 15→5, count 2→1
        CancelOrder{3},                           // removes bid level entirely
    },
    .expected_trades = {},
    .expected_levels = {
        {.side=Side::Sell, .price=100, .total_quantity=5, .order_count=1},
    },
};

void data_driven_suite() {
    run_order_book_test(tc_exact_match);
    run_order_book_test(tc_partial_fill);
    run_order_book_test(tc_fifo_same_price);
    run_order_book_test(tc_multi_level_sweep);
    run_order_book_test(tc_gtc_rests_then_matches);
    run_order_book_test(tc_ioc_partial_fill);
    run_order_book_test(tc_ioc_no_fill);
    run_order_book_test(tc_fok_rejected);
    run_order_book_test(tc_fok_accepted);
    run_order_book_test(tc_sell_sweep_bids);
    run_order_book_test(tc_gtc_partial_fill_rests);
    run_order_book_test(tc_ioc_sell);
    run_order_book_test(tc_fok_sell_rejected);
    run_order_book_test(tc_fok_sell_accepted);
    run_order_book_test(tc_cancel_then_rematch);
    run_order_book_test(tc_multiple_resting_levels);
    run_order_book_test(tc_cancel_order);
}

// ── Summary ───────────────────────────────────────────────────────────────────

int data_driven_summary() {
    const int total = dd::pass_count + dd::fail_count;
    std::cout << "\n───────────────────────── Data-Driven Results ─────────────────────────\n";
    std::cout << "  Passed: " << dd::pass_count << " / " << total << "\n";
    if (dd::fail_count > 0) {
        std::cout << "  Failed: " << dd::fail_count << "\n";
        return 1;
    }
    std::cout << "  All tests passed.\n";
    return 0;
}
