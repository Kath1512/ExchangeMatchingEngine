#pragma once
#include <cstdint>
#include <string_view>

using OrderId = std::uint64_t;
using SequenceNumber = std::uint64_t;
using Price = std::int64_t;
using Quantity = std::int64_t;
using TradeId = std::uint64_t;

enum class Side{
    Buy,
    Sell
};

enum class TimeInForce{
    FillOrKill,
    GoodTillCancel,
    ImmediateOrCancel
};

enum class OrderType{
    Limit,
    Market
};
