#include "networking/input_handler.h"
#include <iostream>
#include <limits>

std::optional<Message> read_message(){
    while(true){
        std::cout << "Enter message type | 1: Add Order | 2: Modify Order | 3: Cancel Order | -1: Quit\n" << std::flush;
        int msg_typ = 0;
        std::cin >> msg_typ;

        if(std::cin.fail()){
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Invalid input, try again\n";
            continue;
        }

        switch (msg_typ){
            case 1: {
                OrderId id;
                Price price;
                Quantity quantity;
                int side, tif;
                std::cout << "client_order_id price quantity side(0=Buy 1=Sell) tif(0=FOK 1=GTC 2=IOC): ";
                std::cin >> id >> price >> quantity >> side >> tif;
                return AddOrder{
                    OrderType::Limit,
                    static_cast<TimeInForce>(tif),
                    id, price, quantity,
                    static_cast<Side>(side)
                };
            }
            case 2: {
                OrderId id;
                Price new_price;
                Quantity new_quantity;
                std::cout << "order_id new_price new_quantity: ";
                std::cin >> id >> new_price >> new_quantity;
                return ModifyOrder{id, new_price, new_quantity};
            }
            case 3: {
                OrderId id;
                std::cout << "order_id: ";
                std::cin >> id;
                return CancelOrder{id};
            }
            case -1:
                return std::nullopt;
            default:
                std::cout << "Unknown type, try again\n";
                continue;
        }
    }
}
