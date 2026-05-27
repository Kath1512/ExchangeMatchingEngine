#include <iostream>
#include "order.h"
#include "price_level.h"
#include "order_book.h"
#include "trade.h"



void test_order(){
    std::cout << "-----Test order-----\n";
    Order order(TimeInForce::FillOrKill, 1512, 15, 10, Side::Buy, 1);
    std::cout << order << "\n";
    order.fill_order(4);
    std::cout << order << "\n";
    order.fill_order(6);
    std::cout << order << "\n";

    std::cout << order.to_string();

    // Order order2(TimeInForce::FillOrKill, 1512, 15, 0, Side::Buy, 1);
}
void test_price_level(){
    std::cout << "-----Test price level-----\n";
    PriceLevel pl(100);
    Order order1(TimeInForce::FillOrKill, 1, 100, 10, Side::Buy, 1);
    Order order2(TimeInForce::GoodTillCancel, 2, 100, 30, Side::Sell, 1);

    pl.add_order(order1);
    pl.add_order(order2);

    std::cout << pl << "\n";

    pl.fill_front_order(10);
    // pl.pop_front_order();
}
void test_order_book1(){
    OrderBook order_book;

    //bids
    Order order1(TimeInForce::FillOrKill, 1, 100, 10, Side::Buy, 1);
    order_book.add_order(order1);
    order_book.add_order(Order(TimeInForce::FillOrKill, 3, 120, 90, Side::Buy, 2));
    order_book.add_order(Order(TimeInForce::FillOrKill, 5, 150, 50, Side::Buy, 2));

    //asks
    Order order2(TimeInForce::GoodTillCancel, 2, 100, 30, Side::Sell, 1);
    order_book.add_order(order2);
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 4, 90, 70, Side::Sell, 3));
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 6, 230, 20, Side::Sell, 2));

    order_book.add_order(Order(TimeInForce::FillOrKill, 7, 120, 500, Side::Buy, 2));
    order_book.add_order(Order(TimeInForce::FillOrKill, 7, 90, 30, Side::Sell, 2));

    std::cout << order_book << "\n";
}
void test_order_book2(){
    OrderBook order_book;

    order_book.add_order(Order(TimeInForce::GoodTillCancel, 1, 100, 10, Side::Sell, 1));
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 2, 100, 20, Side::Sell, 2));
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 3, 105, 15, Side::Sell, 3));
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 4, 110, 10, Side::Sell, 4));

    // Resting bids
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 5, 90, 10, Side::Buy, 5));
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 6, 85, 20, Side::Buy, 6));

    // Incoming buy sweeps asks
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 7, 105, 40, Side::Buy, 7));

    // Incoming sell sweeps bids
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 8, 80, 25, Side::Sell, 8));

    // No-crossing orders rest
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 9, 70, 5, Side::Buy, 9));
    order_book.add_order(Order(TimeInForce::GoodTillCancel, 10, 120, 5, Side::Sell, 10));

    // std::cout << order_book.trades_to_string();
    const auto& trades = order_book.get_trades();
    std::cout << trades << "\n";
    // std::cout << trades[0].get_aggressor_side() << " " << trades[0].get_buyer().get_order_id() << "\n";
    order_book.cancel_order(4);
    order_book.cancel_order(6);
    std::cout << order_book << "\n";
}
void test_order_book3(){
    OrderBook book;

    // Same-price FIFO asks
    book.add_order(Order(TimeInForce::GoodTillCancel, 1, 100, 10, Side::Sell, 1));
    book.add_order(Order(TimeInForce::GoodTillCancel, 2, 100, 20, Side::Sell, 2));

    // Higher ask levels
    book.add_order(Order(TimeInForce::GoodTillCancel, 3, 105, 15, Side::Sell, 3));
    book.add_order(Order(TimeInForce::GoodTillCancel, 4, 110, 10, Side::Sell, 4));

    // Bids
    book.add_order(Order(TimeInForce::GoodTillCancel, 5, 90, 10, Side::Buy, 5));
    book.add_order(Order(TimeInForce::GoodTillCancel, 6, 85, 20, Side::Buy, 6));

    // Buy sweeps asks: tests FIFO + multi-level + partial leftover
    book.add_order(Order(TimeInForce::GoodTillCancel, 7, 105, 40, Side::Buy, 7));

    // Sell sweeps bids: tests bid priority + partial leftover
    book.add_order(Order(TimeInForce::GoodTillCancel, 8, 80, 25, Side::Sell, 8));

    // Resting no-cross orders
    book.add_order(Order(TimeInForce::GoodTillCancel, 9, 70, 5, Side::Buy, 9));
    book.add_order(Order(TimeInForce::GoodTillCancel, 10, 120, 5, Side::Sell, 10));

    // Cancel remaining resting orders
    book.cancel_order(4); // removes ask 110
    book.cancel_order(6); // removes remaining bid 85

    std::cout << book.get_trades() << "\n";
    std::cout << book << "\n";
}
void test_time_in_force() {
    OrderBook book;

    // Resting asks
    book.add_order(Order(TimeInForce::GoodTillCancel, 1, 100, 10, Side::Sell, 1));
    book.add_order(Order(TimeInForce::GoodTillCancel, 2, 105, 10, Side::Sell, 2));

    // IOC: can fill 15, leftover 10 canceled
    book.add_order(Order(TimeInForce::ImmediateOrCancel, 3, 105, 25, Side::Buy, 3));

    // Add more liquidity
    book.add_order(Order(TimeInForce::GoodTillCancel, 4, 110, 20, Side::Sell, 4));

    // FOK: needs 25 @ <=110, only 20 available, should do NOTHING
    book.add_order(Order(TimeInForce::FillOrKill, 5, 110, 25, Side::Buy, 5));

    // GTC: fills 5, leftover 10 rests as bid
    book.add_order(Order(TimeInForce::GoodTillCancel, 6, 110, 15, Side::Buy, 6));

    std::cout << book.get_trades() << "\n";
    std::cout << book << "\n";
}
void test_time_in_force_complex() {
    
}
int main(){
    // test_order();
    // test_price_level();
    test_time_in_force_complex();
}