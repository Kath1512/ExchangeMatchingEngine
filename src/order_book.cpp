#include "orderbook/order_book.h"

std::ostream& operator<<(std::ostream& os, const AddOrderResult& result) {
    os << result.to_string();
    return os;
}
