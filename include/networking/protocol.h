#pragma once
#include <cstdint>
#include <cstring>
#include "orderbook/order_event.h"

// ── Message type tags ─────────────────────────────────────────────────────────

enum class MsgType : uint8_t {
    OrderAccepted  = 1,
    OrderRejected  = 2,
    OrderCanceled  = 3,
    CancelRejected = 4,
    OrderModified  = 5,
    ModifyRejected = 6,
    TradeExecuted  = 7,
};

// ── Header ────────────────────────────────────────────────────────────────────

struct __attribute__((packed)) MsgHeader {
    MsgType  type;
    uint16_t payload_len;
};

// ── Wire structs ──────────────────────────────────────────────────────────────

struct __attribute__((packed)) WireOrderAccepted {
    uint64_t order_id;
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

// ── Serialise ─────────────────────────────────────────────────────────────────
// Returns total bytes written. src must point to start of payload (after header).

// OrderAccepted
inline std::size_t serialise(const OrderAccepted& ev, uint8_t* out) {
    MsgHeader header{ MsgType::OrderAccepted, sizeof(WireOrderAccepted) };
    WireOrderAccepted wire{ ev.order_id };
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

// ── Deserialise ───────────────────────────────────────────────────────────────
// src must point to start of payload (after header).

// OrderAccepted
inline bool deserialise(const uint8_t* src, OrderAccepted& ev) {
    WireOrderAccepted wire;
    std::memcpy(&wire, src, sizeof(wire));
    ev.order_id = wire.order_id;
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
