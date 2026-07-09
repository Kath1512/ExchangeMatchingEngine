# C++ Exchange Matching Engine

## Architecture

```
Client                                Server
──────────────────────────────────────────────────────────
stdin ──► msg_sender ──[TCP]──► msg_parser ──► MessageSink
                                                    │
                                              engine_thread
                                                    │
                                               OrderBook
                                                    │
                                              EventSink
                                                    │
event_consumer ◄── EventSink ◄── event_parser ◄── event_sender
```

**Server threads**
- `msg_parser_thread` — receives binary order messages from client socket → `MessageSink`
- `engine_thread` — drains `MessageSink`, calls `OrderBook::process()`, generates events → `EventSink`
- `event_sender_thread` — drains `EventSink`, serialises events, sends over socket

**Client threads**
- Main thread — reads orders from stdin, serialises, sends to server
- `event_recv_thread` — receives binary events from server → `EventSink`
- `event_consumer_thread` — drains `EventSink`, prints to stdout

---

## How to run

**Step 1 — Build**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
```

**Step 2 — Start server** (Terminal 1)
```bash
./build/server
```
Wait for:
```
Listening on port 8080
```

**Step 3 — Start client** (Terminal 2)
```bash
./build/client
```
Server prints `Client connected`. Enter orders on the client terminal.

---

## Project structure

```
include/
    orderbook/
        types.h              — Price, Quantity, OrderId, Side, TimeInForce aliases
        order_messages.h     — AddOrder, CancelOrder, ModifyOrder, Message variant
        order_event.h        — PrivateEvent, PublicEvent, RoutedEvent, Event variant
        order_book.h         — OrderBook<EventSink> template
        ring_buffer.h        — SPSC lock-free ring buffer
        event_consumer.h     — EventSink alias, consume_events declaration
        message_consumer.h   — MessageSink, DefaultBook aliases, consume_messages declaration
        constant.h           — RING_BUFFER_SIZE
    networking/
        protocol.h           — binary wire protocol (MsgType, OrderMsgType, ser/deser)
        event_sender.h       — run_sender (EventSink → socket)
        event_parser.h       — run_parser (socket → EventSink)
        msg_sender.h         — run_sender (Message → socket)
        msg_parser.h         — run_parser (socket → MessageSink)
        socket_utils.h       — send_all, recv_all
        input_handler.h      — read_message (stdin → Message)
        transport/
            tcp.h            — setup_server, setup_client

networking/
    event_sender.cpp         — pops events from EventSink, serialises, routes to client fd
    event_parser.cpp         — receives event bytes, deserialises, pushes to EventSink
    msg_sender.cpp           — serialises Message, sends over socket
    msg_parser.cpp           — receives order bytes, stamps connection_id, pushes to MessageSink
    input_handler.cpp        — reads one Message from stdin
    transport/tcp.cpp        — TCP server/client setup

src/
    order_book.cpp           — matching engine (price-time priority, GTC/IOC/FOK)
    event_consumer.cpp       — prints events from EventSink to stdout
    message_consumer.cpp     — drains MessageSink into OrderBook
    order_event.cpp          — ostream operators for all event types
    order.cpp / price_level.cpp / trade.cpp / helpers.cpp

server_code/server.cpp       — server main: TCP accept + 3 threads
client_code/client.cpp       — client main: TCP connect + 2 threads + stdin loop
```

---

## Wire protocol

### Client → Server (order messages)

Header: `OrderMsgType (uint8_t) | payload_len (uint16_t)`

| MsgType | Struct | Fields |
|---------|--------|--------|
| `AddOrder = 1` | `WireAddOrder` | order_type, tif, client_order_id, price, quantity, side |
| `CancelOrder = 2` | `WireCancelOrder` | order_id |
| `ModifyOrder = 3` | `WireModifyOrder` | order_id, new_price, new_quantity |

### Server → Client (events)

Header: `MsgType (uint8_t) | payload_len (uint16_t)`

**Private events** (routed to originating client only)

| MsgType | Event | Fields |
|---------|-------|--------|
| `1` | `OrderRested` | order_id, client_order_id, remaining_quantity |
| `2` | `OrderFilled` | order_id, client_order_id |
| `3` | `OrderExpired` | order_id, client_order_id |
| `4` | `OrderRejected` | order_id, reason |
| `5` | `OrderCanceled` | order_id |
| `6` | `CancelRejected` | order_id, reason |
| `7` | `OrderModified` | order_id, new_price, new_quantity |
| `8` | `ModifyRejected` | order_id, reason |

**Public events** (broadcast to all clients)

| MsgType | Event | Fields |
|---------|-------|--------|
| `9` | `TradeExecuted` | buyer_id, seller_id, price, quantity, aggressive_side |
| `10` | `BookUpdate` | side, price, new_total_quantity |

---

## Order entry (stdin on client)

```
Enter message type | 1: Add Order | 2: Modify Order | 3: Cancel Order | -1: Quit
```

| Type | Prompt | Fields |
|------|--------|--------|
| `1` | `client_order_id price quantity side tif` | — |
| `2` | `order_id new_price new_quantity` | — |
| `3` | `order_id` | — |

**side:** `0` = Buy, `1` = Sell  
**tif:** `0` = FOK, `1` = GTC, `2` = IOC

> Note: `client_order_id` is your reference number. The book assigns its own internal `order_id` sequentially.

---

## Test sequence

```
1
1 100 10 0 1       -> AddOrder client_id=1 price=100 qty=10 Buy GTC
                      Events: BookUpdate{Buy,100,10}, OrderRested{order_id=1}

1
2 100 5 1 1        -> AddOrder client_id=2 price=100 qty=5 Sell GTC  (crosses with id=1)
                      Events: TradeExecuted{price=100,qty=5}, BookUpdate{Buy,100,5},
                              OrderFilled{order_id=2}, BookUpdate{Ask,100,0}

1
3 99 20 0 1        -> AddOrder client_id=3 price=99 qty=20 Buy GTC
                      Events: BookUpdate{Buy,99,20}, OrderRested{order_id=3}

2
1 101 5            -> ModifyOrder order_id=1 new_price=101 new_qty=5
                      Events: OrderModified{order_id=1}

3
3                  -> CancelOrder order_id=3
                      Events: BookUpdate{Buy,99,0}, OrderCanceled{order_id=3}

-1                 -> Quit
```
