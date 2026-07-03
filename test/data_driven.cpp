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

void check_levels(const TestBook&                   book,
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

void run_order_book_test(const OrderBookTestCase& tc, bool logging = false) {
    dd::current_test = tc.name;
    std::cout << "[DD] " << tc.name << "\n";

    VectorSink sink;
    TestBook book(sink);
    for (const TestInput& input : tc.inputs) {
        book.process_message(input);
    }

    check_trades(book.get_trades(), tc.expected_trades);
    check_levels(book, tc.expected_levels);

    if(logging){
        std::cout << book.get_trades() << "\n";
        std::cout << book << "\n";
    }
}

// ── Example test cases ────────────────────────────────────────────────────────

// Aliases for readability inside test case definitions.
static constexpr TimeInForce GTC = TimeInForce::GoodTillCancel;
static constexpr TimeInForce IOC = TimeInForce::ImmediateOrCancel;
static constexpr TimeInForce FOK = TimeInForce::FillOrKill;
static constexpr OrderType Limit  = OrderType::Limit;
static constexpr OrderType Market = OrderType::Market;

// ── 1. Exact match ────────────────────────────────────────────────────────────
// Two orders of equal size crossing at the same price.
// Both are fully consumed; book is empty afterward.
static const OrderBookTestCase tc_exact_match {
    .name = "exact_match",
    .inputs = {
        AddOrder{Limit, GTC, 1, 100, 10, Side::Sell},
        AddOrder{Limit, GTC, 2, 100, 10, Side::Buy},
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
        AddOrder{Limit, GTC, 1, 100, 20, Side::Sell},
        AddOrder{Limit, GTC, 2, 100,  8, Side::Buy},
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
        AddOrder{Limit, GTC, 1, 100, 10, Side::Sell},   // arrives first
        AddOrder{Limit, GTC, 2, 100, 15, Side::Sell},   // arrives second
        AddOrder{Limit, GTC, 3, 100, 12, Side::Buy},    // fills 10 from #1, then 2 from #2
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
        AddOrder{Limit, GTC, 1, 100, 10, Side::Sell},
        AddOrder{Limit, GTC, 2, 110,  5, Side::Sell},
        AddOrder{Limit, GTC, 3, 110, 15, Side::Buy},    // crosses both levels
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
        AddOrder{Limit, GTC, 1, 100, 10, Side::Sell},   // rests
        AddOrder{Limit, GTC, 2,  90,  5, Side::Buy},    // 90 < 100: does not cross, rests
        AddOrder{Limit, GTC, 3,  90,  5, Side::Sell},   // crosses bid #2 at 90
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
        AddOrder{Limit, GTC, 1, 100,  5, Side::Sell},
        AddOrder{Limit, IOC, 2, 100, 10, Side::Buy},    // fills 5, cancels leftover 5
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
        AddOrder{Limit, IOC, 1, 100, 10, Side::Buy},    // nothing to match against
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
        AddOrder{Limit, GTC, 1, 100, 10, Side::Sell},
        AddOrder{Limit, FOK, 2, 100, 15, Side::Buy},    // needs 15, only 10 available → rejected
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
        AddOrder{Limit, GTC, 1, 100, 10, Side::Sell},
        AddOrder{Limit, GTC, 2, 105,  5, Side::Sell},
        AddOrder{Limit, FOK, 3, 105, 15, Side::Buy},    // 10@100 + 5@105 = 15 → fills
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
        AddOrder{Limit, GTC, 1, 100, 15, Side::Buy},
        AddOrder{Limit, GTC, 2,  90, 10, Side::Buy},
        AddOrder{Limit, GTC, 3,  85, 20, Side::Sell},   // crosses both bids; 15+5=20
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
        AddOrder{Limit, GTC, 1, 100,  5, Side::Sell},
        AddOrder{Limit, GTC, 2, 100, 12, Side::Buy},    // fills 5, leftover 7 rests as bid
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
        AddOrder{Limit, GTC, 1, 100,  8, Side::Buy},
        AddOrder{Limit, IOC, 2, 100, 15, Side::Sell},   // fills 8, cancels leftover 7
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
        AddOrder{Limit, GTC, 1, 100, 10, Side::Buy},
        AddOrder{Limit, FOK, 2, 100, 15, Side::Sell},   // needs 15, only 10 available
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
        AddOrder{Limit, GTC, 1, 100, 10, Side::Buy},
        AddOrder{Limit, GTC, 2,  90,  5, Side::Buy},
        AddOrder{Limit, FOK, 3,  90, 15, Side::Sell},   // 10+5=15 → fills
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
        AddOrder{Limit, GTC, 1, 100, 10, Side::Sell},
        AddOrder{Limit, GTC, 2,  90,  5, Side::Buy},    // rests; does not cross ask
        CancelOrder{2},
        AddOrder{Limit, GTC, 3, 100, 10, Side::Buy},    // crosses ask 1
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
        AddOrder{Limit, GTC, 1, 110, 10, Side::Sell},
        AddOrder{Limit, GTC, 2, 120,  5, Side::Sell},
        AddOrder{Limit, GTC, 3,  90,  8, Side::Buy},
        AddOrder{Limit, GTC, 4,  80, 12, Side::Buy},
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
        AddOrder{Limit, GTC, 1, 100, 10, Side::Sell},   // first order at @100
        AddOrder{Limit, GTC, 2, 100,  5, Side::Sell},   // second order at @100
        AddOrder{Limit, GTC, 3,  90, 20, Side::Buy},    // sole bid
        CancelOrder{1},                           // @100 level: qty 15→5, count 2→1
        CancelOrder{3},                           // removes bid level entirely
    },
    .expected_trades = {},
    .expected_levels = {
        {.side=Side::Sell, .price=100, .total_quantity=5, .order_count=1},
    },
};

// ── 18. Market buy full fill ──────────────────────────────────────────────────
// Market buy sweeps the entire resting ask. Execution price is the maker's
// price (100), not a price field on the market order.
static const OrderBookTestCase tc_market_buy_full_fill {
    .name = "market_buy_full_fill",
    .inputs = {
        AddOrder{Limit,  GTC, 1, 100, 10, Side::Sell},
        AddOrder{Market, GTC, 2,   0, 10, Side::Buy},   // price=0 is ignored for market orders
    },
    .expected_trades = {
        {.buyer_id=2, .seller_id=1, .price=100, .quantity=10},
    },
    .expected_levels = {},
};

// ── 19. Market buy partial fill ───────────────────────────────────────────────
// Only 5 units are available; market order fills what it can and does not rest.
static const OrderBookTestCase tc_market_buy_partial_fill {
    .name = "market_buy_partial_fill",
    .inputs = {
        AddOrder{Limit,  GTC, 1, 100,  5, Side::Sell},
        AddOrder{Market, GTC, 2,   0, 10, Side::Buy},   // fills 5, remainder gone (no resting)
    },
    .expected_trades = {
        {.buyer_id=2, .seller_id=1, .price=100, .quantity=5},
    },
    .expected_levels = {},
};

// ── 20. Market buy no liquidity ───────────────────────────────────────────────
// Nothing on the ask side — market order executes nothing and does not rest.
static const OrderBookTestCase tc_market_buy_no_liquidity {
    .name = "market_buy_no_liquidity",
    .inputs = {
        AddOrder{Market, GTC, 1, 0, 10, Side::Buy},
    },
    .expected_trades = {},
    .expected_levels = {},
};

// ── 21. Market sell full fill ─────────────────────────────────────────────────
// Market sell hits the best resting bid at maker price.
static const OrderBookTestCase tc_market_sell_full_fill {
    .name = "market_sell_full_fill",
    .inputs = {
        AddOrder{Limit,  GTC, 1, 100, 10, Side::Buy},
        AddOrder{Market, GTC, 2,   0, 10, Side::Sell},
    },
    .expected_trades = {
        {.buyer_id=1, .seller_id=2, .price=100, .quantity=10},
    },
    .expected_levels = {},
};

// ── 22. Market buy multi-level sweep ─────────────────────────────────────────
// Market order sweeps two ask levels regardless of price — no price limit.
// First fills the cheaper level (100), then the more expensive one (110).
static const OrderBookTestCase tc_market_buy_multi_level {
    .name = "market_buy_multi_level",
    .inputs = {
        AddOrder{Limit,  GTC, 1, 100,  5, Side::Sell},
        AddOrder{Limit,  GTC, 2, 110,  5, Side::Sell},
        AddOrder{Market, GTC, 3,   0, 10, Side::Buy},   // sweeps both levels
    },
    .expected_trades = {
        {.buyer_id=3, .seller_id=1, .price=100, .quantity=5},
        {.buyer_id=3, .seller_id=2, .price=110, .quantity=5},
    },
    .expected_levels = {},
};

// ── 23. Market sell partial fill ─────────────────────────────────────────────
// Only 5 units of bid liquidity exist; market sell fills what's available
// and the remainder is discarded (market orders never rest).
static const OrderBookTestCase tc_market_sell_partial_fill {
    .name = "market_sell_partial_fill",
    .inputs = {
        AddOrder{Limit,  GTC, 1, 100,  5, Side::Buy},
        AddOrder{Market, GTC, 2,   0, 10, Side::Sell},  // fills 5, remainder gone
    },
    .expected_trades = {
        {.buyer_id=1, .seller_id=2, .price=100, .quantity=5},
    },
    .expected_levels = {},
};

// ── 24. Market sell no liquidity ──────────────────────────────────────────────
// Nothing on the bid side — market sell executes nothing and does not rest.
static const OrderBookTestCase tc_market_sell_no_liquidity {
    .name = "market_sell_no_liquidity",
    .inputs = {
        AddOrder{Market, GTC, 1, 0, 10, Side::Sell},
    },
    .expected_trades = {},
    .expected_levels = {},
};

// ── 25. Market sell multi-level sweep ────────────────────────────────────────
// Market sell sweeps two bid levels in descending price order (best bid first).
static const OrderBookTestCase tc_market_sell_multi_level {
    .name = "market_sell_multi_level",
    .inputs = {
        AddOrder{Limit,  GTC, 1, 100,  5, Side::Buy},
        AddOrder{Limit,  GTC, 2,  90,  5, Side::Buy},
        AddOrder{Market, GTC, 3,   0, 10, Side::Sell},  // hits @100 first, then @90
    },
    .expected_trades = {
        {.buyer_id=1, .seller_id=3, .price=100, .quantity=5},
        {.buyer_id=2, .seller_id=3, .price=90,  .quantity=5},
    },
    .expected_levels = {},
};

// ── 26. Market buy FIFO within level ─────────────────────────────────────────
// Two resting sells at the same price; market buy must fill the earlier-
// arriving order first (price-time priority).
static const OrderBookTestCase tc_market_buy_fifo {
    .name = "market_buy_fifo",
    .inputs = {
        AddOrder{Limit,  GTC, 1, 100,  6, Side::Sell},  // arrives first
        AddOrder{Limit,  GTC, 2, 100,  6, Side::Sell},  // arrives second
        AddOrder{Market, GTC, 3,   0,  8, Side::Buy},   // fills 6 from #1, then 2 from #2
    },
    .expected_trades = {
        {.buyer_id=3, .seller_id=1, .price=100, .quantity=6},
        {.buyer_id=3, .seller_id=2, .price=100, .quantity=2},
    },
    .expected_levels = {
        {.side=Side::Sell, .price=100, .total_quantity=4, .order_count=1},
    },
};

// ── 27. Market sell FIFO within level ────────────────────────────────────────
// Two resting bids at the same price; market sell must fill the earlier-
// arriving order first.
static const OrderBookTestCase tc_market_sell_fifo {
    .name = "market_sell_fifo",
    .inputs = {
        AddOrder{Limit,  GTC, 1, 100,  6, Side::Buy},   // arrives first
        AddOrder{Limit,  GTC, 2, 100,  6, Side::Buy},   // arrives second
        AddOrder{Market, GTC, 3,   0,  8, Side::Sell},  // fills 6 from #1, then 2 from #2
    },
    .expected_trades = {
        {.buyer_id=1, .seller_id=3, .price=100, .quantity=6},
        {.buyer_id=2, .seller_id=3, .price=100, .quantity=2},
    },
    .expected_levels = {
        {.side=Side::Buy, .price=100, .total_quantity=4, .order_count=1},
    },
};

// ── 28. Market buy after cancel ───────────────────────────────────────────────
// A resting ask is cancelled; a subsequent market buy fills only against
// the remaining liquidity, confirming the book is consistent post-cancel.
static const OrderBookTestCase tc_market_buy_after_cancel {
    .name = "market_buy_after_cancel",
    .inputs = {
        AddOrder{Limit,  GTC, 1, 100, 10, Side::Sell},
        AddOrder{Limit,  GTC, 2, 100,  5, Side::Sell},
        CancelOrder{1},                                  // remove first ask; only #2 remains
        AddOrder{Market, GTC, 3,   0,  5, Side::Buy},   // fills entirely against #2
    },
    .expected_trades = {
        {.buyer_id=3, .seller_id=2, .price=100, .quantity=5},
    },
    .expected_levels = {},
};

static const OrderBookTestCase tc_all_in_one_market_and_limit_orders {
    .name = "all_in_one_market_and_limit_order",

    .inputs = {
        AddOrder{OrderType::Limit,  TimeInForce::GoodTillCancel, 1, 100, 10, Side::Sell},
        AddOrder{OrderType::Limit,  TimeInForce::GoodTillCancel, 2, 105, 20, Side::Sell},
        AddOrder{OrderType::Limit,  TimeInForce::GoodTillCancel, 3, 110, 30, Side::Sell},

        AddOrder{OrderType::Limit,  TimeInForce::GoodTillCancel, 4, 95, 15, Side::Buy},
        AddOrder{OrderType::Limit,  TimeInForce::GoodTillCancel, 5, 90, 20, Side::Buy},

        AddOrder{OrderType::Market, TimeInForce::ImmediateOrCancel, 6, 0, 25, Side::Buy},
        AddOrder{OrderType::Market, TimeInForce::ImmediateOrCancel, 7, 0, 20, Side::Sell},

        AddOrder{OrderType::Limit,  TimeInForce::GoodTillCancel, 8, 105, 20, Side::Buy},

        AddOrder{OrderType::Market, TimeInForce::ImmediateOrCancel, 9, 0, 40, Side::Sell},

        AddOrder{OrderType::Limit,  TimeInForce::GoodTillCancel, 10, 120, 5, Side::Sell},
    },

    .expected_trades = {
        {.buyer_id=6, .seller_id=1, .price=100, .quantity=10},
        {.buyer_id=6, .seller_id=2, .price=105, .quantity=15},

        {.buyer_id=4, .seller_id=7, .price=95, .quantity=15},
        {.buyer_id=5, .seller_id=7, .price=90, .quantity=5},

        {.buyer_id=8, .seller_id=2, .price=105, .quantity=5},
        {.buyer_id=8, .seller_id=9, .price=105, .quantity=15},
        {.buyer_id=5, .seller_id=9, .price=90, .quantity=15},
    },

    .expected_levels = {
        {Side::Sell, 110, 30, 1},
        {Side::Sell, 120, 5, 1},
    },
};

// ── 30. ERR-002 regression: cancel after full fill ────────────────────────────
// Order 1 is fully consumed by order 2. A subsequent cancel of order 1's ID
// must be a no-op (false return) — not UB from a dangling iterator.
// Verified by: no crash, book is empty, only one trade recorded.
static const OrderBookTestCase tc_cancel_after_fill {
    .name = "cancel_after_fill",
    .inputs = {
        AddOrder{Limit, GTC, 1, 100, 10, Side::Sell},
        AddOrder{Limit, GTC, 2, 100, 10, Side::Buy},   // fully fills order 1
        CancelOrder{1},                                 // stale ID — must be a no-op
    },
    .expected_trades = {
        {.buyer_id=2, .seller_id=1, .price=100, .quantity=10},
    },
    .expected_levels = {},
};

// ── 31. Modify: price change ──────────────────────────────────────────────────
// A resting ask is moved from price 100 to price 110.
// The old level disappears; the new level appears with the same quantity.
static const OrderBookTestCase tc_modify_price {
    .name = "modify_price",
    .inputs = {
        AddOrder{Limit, GTC, 1, 100, 10, Side::Sell},
        ModifyOrder{1, 110, 10},
    },
    .expected_trades = {},
    .expected_levels = {
        {.side=Side::Sell, .price=110, .total_quantity=10, .order_count=1},
    },
};

// ── 32. Modify: quantity reduction ───────────────────────────────────────────
// Reducing the quantity of a resting ask; price is unchanged.
static const OrderBookTestCase tc_modify_quantity {
    .name = "modify_quantity",
    .inputs = {
        AddOrder{Limit, GTC, 1, 100, 20, Side::Sell},
        ModifyOrder{1, 100, 8},
    },
    .expected_trades = {},
    .expected_levels = {
        {.side=Side::Sell, .price=100, .total_quantity=8, .order_count=1},
    },
};

// ── 33. Modify loses time priority ────────────────────────────────────────────
// Two sells rest at the same price. Order 1 (earlier) is modified (cancel +
// re-add), putting it behind order 2. The incoming buy fills order 2 first.
static const OrderBookTestCase tc_modify_loses_priority {
    .name = "modify_loses_priority",
    .inputs = {
        AddOrder{Limit, GTC, 1, 100, 10, Side::Sell},   // arrives first
        AddOrder{Limit, GTC, 2, 100, 10, Side::Sell},   // arrives second
        ModifyOrder{1, 100, 10},                         // re-added: now behind order 2
        AddOrder{Limit, GTC, 3, 100, 10, Side::Buy},    // must fill order 2 first
    },
    .expected_trades = {
        {.buyer_id=3, .seller_id=2, .price=100, .quantity=10},
    },
    .expected_levels = {
        {.side=Side::Sell, .price=100, .total_quantity=10, .order_count=1},
    },
};

// ── 34. Modify crosses the book ───────────────────────────────────────────────
// A resting bid at 100 is modified to 110, which crosses the resting ask at
// 110. The modify causes an immediate match; book is empty afterward.
static const OrderBookTestCase tc_modify_crosses_book {
    .name = "modify_crosses_book",
    .inputs = {
        AddOrder{Limit, GTC, 1, 110, 10, Side::Sell},   // rests
        AddOrder{Limit, GTC, 2, 100, 10, Side::Buy},    // rests; 100 < 110, no cross
        ModifyOrder{2, 110, 10},                         // raises bid to 110 — crosses ask
    },
    .expected_trades = {
        {.buyer_id=2, .seller_id=1, .price=110, .quantity=10},
    },
    .expected_levels = {},
};

// ── 35. Modify nonexistent order ──────────────────────────────────────────────
// Modifying an unknown order ID is a no-op; the book state is unchanged.
static const OrderBookTestCase tc_modify_nonexistent {
    .name = "modify_nonexistent",
    .inputs = {
        AddOrder{Limit, GTC, 1, 100, 10, Side::Sell},
        ModifyOrder{99, 100, 5},                         // ID 99 does not exist
    },
    .expected_trades = {},
    .expected_levels = {
        {.side=Side::Sell, .price=100, .total_quantity=10, .order_count=1},
    },
};

// ── 36. process_message: all three message types in one sequence ──────────────
// Sends AddOrder, ModifyOrder, and CancelOrder all through process_message to
// verify the variant dispatch routes each alternative correctly.
//   - Order 1 (sell @100) is added, then moved to @110 via ModifyOrder.
//   - Order 2 (sell @105) is added, then cancelled via CancelOrder.
//   - Order 3 (buy @110) is added and crosses the modified order 1 at @110.
// Expected: one trade (3 buys from 1 at 110), book empty afterward.
static const OrderBookTestCase tc_process_message_dispatch {
    .name = "process_message_dispatch",
    .inputs = {
        AddOrder{Limit, GTC, 1, 100, 10, Side::Sell},
        AddOrder{Limit, GTC, 2, 105,  5, Side::Sell},
        ModifyOrder{1, 110, 10},                         // moves order 1: @100 → @110
        CancelOrder{2},                                  // removes order 2
        AddOrder{Limit, GTC, 3, 110, 10, Side::Buy},    // crosses order 1 at 110
    },
    .expected_trades = {
        {.buyer_id=3, .seller_id=1, .price=110, .quantity=10},
    },
    .expected_levels = {},
};

// ── 37. process_message: interleaved adds and modifies ────────────────────────
// Stress-tests the dispatch path with a longer sequence where orders are
// modified multiple times before a final sweep clears the book.
//   - Order 1 starts @90, is bumped to @95, then to @100.
//   - Order 2 starts @100 and is never touched.
//   - Market buy consumes both at their final prices (FIFO within @100 level:
//     order 2 arrived first at that price, order 1 re-queued behind it).
static const OrderBookTestCase tc_process_message_multi_modify {
    .name = "process_message_multi_modify",
    .inputs = {
        AddOrder{Limit, GTC, 1,  90, 10, Side::Sell},   // rests @90
        AddOrder{Limit, GTC, 2, 100, 10, Side::Sell},   // rests @100
        ModifyOrder{1,  95, 10},                         // order 1: @90 → @95
        ModifyOrder{1, 100, 10},                         // order 1: @95 → @100 (behind order 2)
        AddOrder{Market, GTC, 3, 0, 20, Side::Buy},     // sweeps @100: fills 2 first, then 1
    },
    .expected_trades = {
        {.buyer_id=3, .seller_id=2, .price=100, .quantity=10},
        {.buyer_id=3, .seller_id=1, .price=100, .quantity=10},
    },
    .expected_levels = {},
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
    run_order_book_test(tc_market_buy_full_fill);
    run_order_book_test(tc_market_buy_partial_fill);
    run_order_book_test(tc_market_buy_no_liquidity);
    run_order_book_test(tc_market_sell_full_fill);
    run_order_book_test(tc_market_buy_multi_level);
    run_order_book_test(tc_market_sell_partial_fill);
    run_order_book_test(tc_market_sell_no_liquidity);
    run_order_book_test(tc_market_sell_multi_level);
    run_order_book_test(tc_market_buy_fifo);
    run_order_book_test(tc_market_sell_fifo);
    run_order_book_test(tc_market_buy_after_cancel);
    run_order_book_test(tc_all_in_one_market_and_limit_orders);
    // ERR-002 regression + event-driven process() + modify behaviour
    run_order_book_test(tc_cancel_after_fill);
    run_order_book_test(tc_modify_price);
    run_order_book_test(tc_modify_quantity);
    run_order_book_test(tc_modify_loses_priority);
    run_order_book_test(tc_modify_crosses_book);
    run_order_book_test(tc_modify_nonexistent);
    // process_message dispatch
    run_order_book_test(tc_process_message_dispatch);
    run_order_book_test(tc_process_message_multi_modify);
}
void run_one(){
    run_order_book_test(tc_all_in_one_market_and_limit_orders, true);
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
