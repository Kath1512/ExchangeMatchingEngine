#pragma once
#include <cstdint>
#include <string_view>
#include <atomic>

using Size = std::size_t;
using OrderId = std::uint64_t;
using SequenceNumber = std::uint64_t;
using Price = std::int64_t;
using Quantity = std::int64_t;
using TradeId = std::uint64_t;
using AtomicBool = std::atomic<bool>;
using Counter = std::int64_t;
using ErrorCode = std::uint8_t;

enum class Side {
    Buy,
    Sell
};

enum class TimeInForce {
    FillOrKill,
    GoodTillCancel,
    ImmediateOrCancel
};

enum class OrderType {
    Limit,
    Market
};

enum class AddOrderStatus {
    FullyFilled,
    PartiallyFilled,
    Resting,
    Failed,
    Rejected
};
