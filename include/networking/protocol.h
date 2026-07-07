#pragma once
#include <cstdint>
#include <cstring>
#include "orderbook/order_event.h"

// ── Message type tags ─────────────────────────────────────────────────────────

enum class MsgType : uint8_t {
    OrderRested    = 1,
    OrderFilled    = 2,
    OrderExpired   = 3,
    OrderRejected  = 4,
    OrderCanceled  = 5,
    CancelRejected = 6,
    OrderModified  = 7,
    ModifyRejected = 8,
    TradeExecuted  = 9,
    BookUpdate     = 10,
};

// ── Header ────────────────────────────────────────────────────────────────────

struct __attribute__((packed)) MsgHeader {
    MsgType  type;
    uint16_t payload_len;
};

// ── Wire structs ──────────────────────────────────────────────────────────────

struct __attribute__((packed)) WireOrderRested {
    uint64_t order_id;
    uint64_t client_order_id;
    int64_t  remaining_quantity;
};

struct __attribute__((packed)) WireOrderFilled {
    uint64_t order_id;
    uint64_t client_order_id;
};

struct __attribute__((packed)) WireOrderExpired {
    uint64_t order_id;
    uint64_t client_order_id;
};

struct __attribute__((packed)) WireOrderRejected {
    uint64_t order_id;
    uint8_t  reason;
};

struct __attribute__((packed)) WireOrderCanceled {
    uint64_t order_id;
};

struct __attribute__((packed)) WireCancelRejected {
    uint64_t order_id;
    uint8_t  reason;
};

struct __attribute__((packed)) WireOrderModified {
    uint64_t order_id;
    int64_t  new_price;
    int64_t  new_quantity;
};

struct __attribute__((packed)) WireModifyRejected {
    uint64_t order_id;
    uint8_t  reason;
};

struct __attribute__((packed)) WireTradeExecuted {
    uint64_t buyer_id;
    uint64_t seller_id;
    int64_t  price;
    int64_t  quantity;
    uint8_t  aggressive_side;
};

struct __attribute__((packed)) WireBookUpdate {
    uint8_t  side;
    int64_t  price;
    int64_t  new_total_quantity;
};

// ── Serialise ─────────────────────────────────────────────────────────────────
// Returns total bytes written. src must point to start of payload (after header).

// OrderRested
inline std::size_t serialise(const OrderRested& ev, uint8_t* out) {
    MsgHeader header{ MsgType::OrderRested, sizeof(WireOrderRested) };
    WireOrderRested wire{ ev.order_id, ev.client_order_id, ev.remaining_quantity };
    std::memcpy(out, &header, sizeof(header));
    std::memcpy(out + sizeof(header), &wire, sizeof(wire));
    return sizeof(header) + sizeof(wire);
}

// OrderFilled
inline std::size_t serialise(const OrderFilled& ev, uint8_t* out) {
    MsgHeader header{ MsgType::OrderFilled, sizeof(WireOrderFilled) };
    WireOrderFilled wire{ ev.order_id, ev.client_order_id };
    std::memcpy(out, &header, sizeof(header));
    std::memcpy(out + sizeof(header), &wire, sizeof(wire));
    return sizeof(header) + sizeof(wire);
}

// OrderExpired
inline std::size_t serialise(const OrderExpired& ev, uint8_t* out) {
    MsgHeader header{ MsgType::OrderExpired, sizeof(WireOrderExpired) };
    WireOrderExpired wire{ ev.order_id, ev.client_order_id };
    std::memcpy(out, &header, sizeof(header));
    std::memcpy(out + sizeof(header), &wire, sizeof(wire));
    return sizeof(header) + sizeof(wire);
}

// OrderRejected
inline std::size_t serialise(const OrderRejected& ev, uint8_t* out) {
    MsgHeader header{ MsgType::OrderRejected, sizeof(WireOrderRejected) };
    WireOrderRejected wire{ ev.order_id, static_cast<uint8_t>(ev.reason) };
    std::memcpy(out, &header, sizeof(header));
    std::memcpy(out + sizeof(header), &wire, sizeof(wire));
    return sizeof(header) + sizeof(wire);
}

// OrderCanceled
inline std::size_t serialise(const OrderCanceled& ev, uint8_t* out) {
    MsgHeader header{ MsgType::OrderCanceled, sizeof(WireOrderCanceled) };
    WireOrderCanceled wire{ ev.order_id };
    std::memcpy(out, &header, sizeof(header));
    std::memcpy(out + sizeof(header), &wire, sizeof(wire));
    return sizeof(header) + sizeof(wire);
}

// CancelRejected
inline std::size_t serialise(const CancelRejected& ev, uint8_t* out) {
    MsgHeader header{ MsgType::CancelRejected, sizeof(WireCancelRejected) };
    WireCancelRejected wire{ ev.order_id, static_cast<uint8_t>(ev.reason) };
    std::memcpy(out, &header, sizeof(header));
    std::memcpy(out + sizeof(header), &wire, sizeof(wire));
    return sizeof(header) + sizeof(wire);
}

// OrderModified
inline std::size_t serialise(const OrderModified& ev, uint8_t* out) {
    MsgHeader header{ MsgType::OrderModified, sizeof(WireOrderModified) };
    WireOrderModified wire{ ev.order_id, ev.new_price, ev.new_quantity };
    std::memcpy(out, &header, sizeof(header));
    std::memcpy(out + sizeof(header), &wire, sizeof(wire));
    return sizeof(header) + sizeof(wire);
}

// ModifyRejected
inline std::size_t serialise(const ModifyRejected& ev, uint8_t* out) {
    MsgHeader header{ MsgType::ModifyRejected, sizeof(WireModifyRejected) };
    WireModifyRejected wire{ ev.order_id, static_cast<uint8_t>(ev.reason) };
    std::memcpy(out, &header, sizeof(header));
    std::memcpy(out + sizeof(header), &wire, sizeof(wire));
    return sizeof(header) + sizeof(wire);
}

// TradeExecuted
inline std::size_t serialise(const TradeExecuted& ev, uint8_t* out) {
    MsgHeader header{ MsgType::TradeExecuted, sizeof(WireTradeExecuted) };
    WireTradeExecuted wire{
        ev.buyer_id,
        ev.seller_id,
        ev.price,
        ev.quantity,
        static_cast<uint8_t>(ev.aggressive_side == Side::Buy ? 0 : 1)
    };
    std::memcpy(out, &header, sizeof(header));
    std::memcpy(out + sizeof(header), &wire, sizeof(wire));
    return sizeof(header) + sizeof(wire);
}

// BookUpdate
inline std::size_t serialise(const BookUpdate& ev, uint8_t* out) {
    MsgHeader header{ MsgType::BookUpdate, sizeof(WireBookUpdate) };
    WireBookUpdate wire{
        static_cast<uint8_t>(ev.side == Side::Buy ? 0 : 1),
        ev.price,
        ev.new_total_quantity
    };
    std::memcpy(out, &header, sizeof(header));
    std::memcpy(out + sizeof(header), &wire, sizeof(wire));
    return sizeof(header) + sizeof(wire);
}

// ── Deserialise ───────────────────────────────────────────────────────────────
// src must point to start of payload (after header).

// OrderRested
inline bool deserialise(const uint8_t* src, OrderRested& ev) {
    WireOrderRested wire;
    std::memcpy(&wire, src, sizeof(wire));
    ev.order_id           = wire.order_id;
    ev.client_order_id    = wire.client_order_id;
    ev.remaining_quantity = wire.remaining_quantity;
    return true;
}

// OrderFilled
inline bool deserialise(const uint8_t* src, OrderFilled& ev) {
    WireOrderFilled wire;
    std::memcpy(&wire, src, sizeof(wire));
    ev.order_id        = wire.order_id;
    ev.client_order_id = wire.client_order_id;
    return true;
}

// OrderExpired
inline bool deserialise(const uint8_t* src, OrderExpired& ev) {
    WireOrderExpired wire;
    std::memcpy(&wire, src, sizeof(wire));
    ev.order_id        = wire.order_id;
    ev.client_order_id = wire.client_order_id;
    return true;
}

// OrderRejected
inline bool deserialise(const uint8_t* src, OrderRejected& ev) {
    WireOrderRejected wire;
    std::memcpy(&wire, src, sizeof(wire));
    ev.order_id = wire.order_id;
    ev.reason   = static_cast<RejectReason>(wire.reason);
    return true;
}

// OrderCanceled
inline bool deserialise(const uint8_t* src, OrderCanceled& ev) {
    WireOrderCanceled wire;
    std::memcpy(&wire, src, sizeof(wire));
    ev.order_id = wire.order_id;
    return true;
}

// CancelRejected
inline bool deserialise(const uint8_t* src, CancelRejected& ev) {
    WireCancelRejected wire;
    std::memcpy(&wire, src, sizeof(wire));
    ev.order_id = wire.order_id;
    ev.reason   = static_cast<RejectReason>(wire.reason);
    return true;
}

// OrderModified
inline bool deserialise(const uint8_t* src, OrderModified& ev) {
    WireOrderModified wire;
    std::memcpy(&wire, src, sizeof(wire));
    ev.order_id     = wire.order_id;
    ev.new_price    = wire.new_price;
    ev.new_quantity = wire.new_quantity;
    return true;
}

// ModifyRejected
inline bool deserialise(const uint8_t* src, ModifyRejected& ev) {
    WireModifyRejected wire;
    std::memcpy(&wire, src, sizeof(wire));
    ev.order_id = wire.order_id;
    ev.reason   = static_cast<RejectReason>(wire.reason);
    return true;
}

// TradeExecuted
inline bool deserialise(const uint8_t* src, TradeExecuted& ev) {
    WireTradeExecuted wire;
    std::memcpy(&wire, src, sizeof(wire));
    ev.buyer_id        = wire.buyer_id;
    ev.seller_id       = wire.seller_id;
    ev.price           = wire.price;
    ev.quantity        = wire.quantity;
    ev.aggressive_side = wire.aggressive_side == 0 ? Side::Buy : Side::Sell;
    return true;
}

// BookUpdate
inline bool deserialise(const uint8_t* src, BookUpdate& ev) {
    WireBookUpdate wire;
    std::memcpy(&wire, src, sizeof(wire));
    ev.side               = wire.side == 0 ? Side::Buy : Side::Sell;
    ev.price              = wire.price;
    ev.new_total_quantity = wire.new_total_quantity;
    return true;
}
