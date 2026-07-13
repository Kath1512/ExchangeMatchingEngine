# C++ Exchange Matching Engine

## Architecture

### Multi-client server — single kqueue loop, many connections

```
 Client A (fd 5)        Client B (fd 6)        Client C (fd 7)
 stdin ► msg_sender     stdin ► msg_sender     stdin ► msg_sender
     │  [TCP send]          │  [TCP send]          │  [TCP send]
     ▼                      ▼                      ▼
 ┌──────────────────────────────────────────────────────────────────┐
 │                    kqueue()  — one fd, one thread                │
 │                                                                    │
 │   kevent() blocks, wakes with a batch of ready fds:               │
 │     [listen_fd] [fd 5] [fd 7]              (fd 6 idle this tick)  │
 │                                                                    │
 │   listen_fd ready ──► accept() ──► set_non_blocking()             │
 │                       ──► fd_to_state.insert({fd, ClientState})   │
 │                       ──► kevent(EV_ADD, fd)   [mutex-guarded]    │
 │                                                                    │
 │   client fd ready ──► fd_to_state.at(fd)   (per-connection state) │
 │        │                                                          │
 │        ▼                                                          │
 │   ┌─────────────────────────────────────────────────┐            │
 │   │ ClientState (fd 5)     ClientState (fd 6)   ...  │            │
 │   │ ┌───────────────┐      ┌───────────────┐         │            │
 │   │ │ buf[STATE_SIZE]│      │ buf[STATE_SIZE]│        │            │
 │   │ │ read_pos ▲     │      │ read_pos ▲     │        │            │
 │   │ │ write_pos ▲    │      │ write_pos ▲    │        │            │
 │   │ │ ParseState:    │      │ ParseState:    │        │            │
 │   │ │  ReadingHeader │      │  ReadingPayload│        │            │
 │   │ │  /ReadingPayload      │  (resumed here)│        │            │
 │   │ │ pending_header │      │ pending_header │        │            │
 │   │ └───────────────┘      └───────────────┘         │            │
 │   └─────────────────────────────────────────────────┘            │
 │        │                                                          │
 │        │  pre_check() → recv_nb_til_limit() → parse_ready_client()│
 │        │  parses up to MSG_LIMIT_PER_CONN complete messages       │
 │        │  per wakeup (fairness — one chatty client can't          │
 │        │  starve fd 6/fd 7), each stamped with connection_id=fd   │
 │        ▼                                                          │
 └──────────────────────────────  push()  ───────────────────────────┘
                                    │
                                    ▼
                              MessageSink (SPSC ring buffer)
                                    │
                              engine_thread
                                    │  OrderBook::process()
                                    ▼
                               EventSink
                                    │
                          event_sender_thread
                                    │
              ┌─────────────────────┴─────────────────────┐
              ▼                                             ▼
      RoutedEvent{connection_id}                     PublicEvent
      send straight to that fd                 broadcast to every fd in
      (e.g. OrderRested → fd 5 only)           fd_to_state (TradeExecuted,
                                                 BookUpdate → fd 5,6,7)
              │                                             │
              ▼                                             ▼
        Client A only                          Client A + Client B + Client C
     event_recv_thread ► EventSink ► event_consumer_thread ► stdout
```

`fd_to_state` (the `unordered_map<int, ClientState>`) is the one piece of state touched from two different threads — the kqueue loop inserts on accept and erases on disconnect, while `event_sender_thread` iterates it to broadcast. Both sides take the same `std::mutex` around those operations.

**Server**
- Main thread — runs the kqueue accept/read loop (`setup_server`). One `kqueue()` handles every connected client: on wakeup, it non-blocking `recv()`s into that client's buffer and drives its per-client `ClientState` state machine to parse as many complete messages as the buffered bytes allow (capped by `MSG_LIMIT_PER_CONN` per wakeup, so one busy client can't starve the others). Completed messages are pushed to `MessageSink`.
- `engine_thread` — drains `MessageSink`, calls `OrderBook::process()`, generates events → `EventSink`.
- `event_sender_thread` — drains `EventSink`, serialises events, and either sends a private event straight to the originating client's fd or broadcasts a public event to every fd in the shared connection map (`fd_to_state`). That map is written by the kqueue loop (accept/disconnect) and read by this thread, so both sides take a shared `std::mutex` around it.

**Client**
- Main thread — reads orders from stdin (reprompts on invalid/unknown input instead of quitting), serialises, sends to server.
- `event_recv_thread` — receives binary events from server → `EventSink`.
- `event_consumer_thread` — drains `EventSink`, prints to stdout.

---

## Non-blocking parsing (`ClientState`)

Each connected client owns one `ClientState`:

- A fixed byte buffer (`STATE_SIZE` bytes) with `read_pos`/`write_pos` cursors — bytes between them are received but not yet parsed.
- A `ParseState` (`ReadingHeader` / `ReadingPayload`) so parsing can resume across kqueue wakeups instead of blocking.
- The in-progress `OrderMsgHeader`, kept once read so a resumed parse doesn't need to re-read it.

`pre_check()` compacts the buffer before each `recv()`: if everything's been consumed it just resets both cursors (O(1)); if the tail is full but a partial message remains, it `memmove`s the unconsumed bytes to the front.

An unrecognised message type doesn't kill the connection — `payload_len` bytes are drained (same atomic all-or-nothing read as any known payload, so it correctly waits across wakeups for the rest to arrive) and the parser resyncs at the next header.

---

## How to run

**Step 1 — Build**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
```

**Step 2 — Start server** (Terminal 1)
```bash
./build/server_kqueue
```
Wait for:
```
Listening on port 8080
```

**Step 3 — Start client(s)** (Terminal 2, 3, ...)
```bash
./build/client
```
Server prints `Client connected` for each one. Enter orders on any client terminal — the server handles multiple clients concurrently over a single kqueue loop.

---

## Project structure

```
include/
    orderbook/
        types.h              — Price, Quantity, OrderId, Side, TimeInForce aliases
        order_messages.h     — AddOrder, CancelOrder, ModifyOrder, Message variant
        order_event.h        — PrivateEvent, PublicEvent, RoutedEvent, Event variant
        order_book.h          — OrderBook<EventSink> template
        ring_buffer.h        — SPSC lock-free ring buffer
        event_consumer.h     — EventSink alias, consume_events declaration
        message_consumer.h   — MessageSink, DefaultBook aliases, consume_messages declaration
        constant.h           — RING_BUFFER_SIZE, STATE_SIZE, MSG_LIMIT_PER_CONN
    networking/
        protocol.h           — binary wire protocol (MsgType, OrderMsgType, ser/deser)
        client_state.h       — ClientState: per-connection buffer, cursors, parse state
        event_sender.h       — run_sender (EventSink → socket, private route / public broadcast)
        event_parser.h       — run_parser (socket → EventSink)
        msg_sender.h         — run_sender (Message → socket)
        msg_parser.h         — parse_ready_client (ClientState → MessageSink)
        socket_utils.h       — send_all, recv_all, recv_nb_til_limit
        input_handler.h      — read_message (stdin → Message)
        transport/
            tcp.h            — setup_server (kqueue loop), setup_client

networking/
    client_state.cpp         — ClientState method implementations
    event_sender.cpp         — pops events from EventSink, serialises, routes/broadcasts
    event_parser.cpp         — receives event bytes, deserialises, pushes to EventSink
    msg_sender.cpp           — serialises Message, sends over socket
    msg_parser.cpp           — parses buffered bytes into Messages via the ClientState machine
    input_handler.cpp        — reads one Message from stdin
    transport/tcp.cpp        — kqueue-based multi-client server; TCP client connect

src/
    order_book.cpp           — matching engine (price-time priority, GTC/IOC/FOK)
    event_consumer.cpp       — prints events from EventSink to stdout
    message_consumer.cpp     — drains MessageSink into OrderBook
    order_event.cpp          — ostream operators for all event types
    order.cpp / price_level.cpp / trade.cpp / helpers.cpp

server_code/multi_server_kqueue.cpp — server main: kqueue accept/parse loop + engine + sender threads
client_code/client.cpp               — client main: TCP connect + 2 threads + stdin loop
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
> Anything other than `1`, `2`, `3`, or `-1` (or non-numeric input) just reprompts — it no longer quits the client.

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
