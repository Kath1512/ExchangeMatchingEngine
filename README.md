# Server/Client Test — Manual Order Entry

## How to run

**Step 1 — Build**

From project root:
```bash
bash run_command/run_test_server_client_order_entry.sh
```
This compiles `build/server` and `build/client`.

**Step 2 — Start server** (Terminal 1)
```bash
./build/server
```
Wait until you see:
```
Listening on port 8080
```

**Step 3 — Start client** (Terminal 2)
```bash
./build/client
```
Server will print `Client connected`. Events will print on the client terminal as orders are processed.

---

## Message format

The server reads from stdin. Each message is two lines:

```
<type>
<fields>
```

| Type | Fields |
|------|--------|
| `1` — Add Order | `order_id price quantity side tif` |
| `2` — Modify Order | `order_id new_price new_quantity` |
| `3` — Cancel Order | `order_id` |
| `-1` — Quit | (no fields) |

**side:** `0` = Buy, `1` = Sell  
**tif:** `0` = FOK, `1` = GTC, `2` = IOC

---

## Test sequence

```
1
1 100 10 0 1       -> AddOrder id=1 price=100 qty=10 Buy GTC

1
2 100 5 1 1        -> AddOrder id=2 price=100 qty=5 Sell GTC  (partial fill vs id=1)

1
3 99 20 0 1        -> AddOrder id=3 price=99 qty=20 Buy GTC

2
1 101 5            -> ModifyOrder id=1 new_price=101 new_qty=5

3
3                  -> CancelOrder id=3

1
4 99 20 1 2        -> AddOrder id=4 price=99 qty=20 Sell IOC  (sweeps resting bids)

-1                 -> Quit
```

---

## Expected events on client

- `OrderAccepted` — id=1 rests on book
- `TradeExecuted` — id=1 vs id=2, qty=5 at price=100
- `OrderAccepted` — id=3 rests on book
- `OrderModified` — id=1 updated to price=101 qty=5
- `OrderCanceled` — id=3 removed
- `TradeExecuted` — id=4 sweeps resting bids (IOC, cancelled if unfilled)
