// #include <iostream>
// #include "orderbook/order.h"
// #include "orderbook/price_level.h"
// #include "orderbook/order_book.h"
// #include "orderbook/trade.h"
// #include "engine/event_consumer.h"
// #include <thread>
// void test(){
//     OrderBook book;
//     // Resting asks
//     book.add_order(Order(OrderType::Limit, TimeInForce::GoodTillCancel, 1, 100, 10, Side::Buy, 1));
//     book.add_order(Order(OrderType::Limit, TimeInForce::GoodTillCancel, 2, 50, 30, Side::Buy, 2));
//     book.add_order(Order(OrderType::Limit, TimeInForce::GoodTillCancel, 3, 80, 10, Side::Buy, 2));
//     book.add_order(Order(OrderType::Limit, TimeInForce::GoodTillCancel, 4, 90, 10, Side::Buy, 2));

//     book.modify_order(1, 150, 20);
//     book.modify_order(2, 5, 100);
//     std::cout << book.cancel_order(2) << "\n";
//     std::cout << book.cancel_order(1) << "\n";
//     std::cout << book.modify_order(3, 200, 200) << "\n";
//     std::cout << book << "\n";
// }

// void test_event(){
//     OrderBook book;
//     std::vector<Message> messages = {
//         AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 1, 100, 10, Side::Sell},
//         AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 2, 105, 20, Side::Sell},
//         AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 3, 110, 15, Side::Sell},
//         AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 4, 90, 20, Side::Buy},
//         AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 5, 85, 15, Side::Buy},

//         ModifyOrder{4, 102, 20},

//         AddOrder{OrderType::Market, TimeInForce::ImmediateOrCancel, 6, 0, 25, Side::Buy},

//         CancelOrder{5},

//         AddOrder{OrderType::Limit, TimeInForce::GoodTillCancel, 7, 100, 30, Side::Sell}
//     };

//     AtomicBool running(true);

//     std::thread consumer(
//         consume_events,
//         std::ref(book),
//         std::ref(running)
//     );

//     for (const auto& msg : messages) {
//         book.process_message(msg);
//     }

//     running = false;
//     consumer.join();

//     std::cout << book << "\n";
// }
// int main(){
//     test_event();
// }

int main() {}