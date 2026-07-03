#pragma once
#include <string_view>
#include "types.h"
//view enum string
constexpr std::string_view to_string(const Side& side){
    switch(side){
        case Side::Buy:
            return "Buy";
        case Side::Sell:
            return "Sell";
    }
    return "unknown";
}

constexpr std::string_view to_string(const OrderType& order_type){
    switch(order_type){
        case OrderType::Limit:
            return "Limit";
        case OrderType::Market:
            return "Market";
    }
    return "unknown";
}

constexpr std::string_view to_string(const TimeInForce& time_in_force){
    switch(time_in_force){
        case TimeInForce::FillOrKill:
            return "FillOrKill";
        case TimeInForce::GoodTillCancel:
            return "GoodTillCancel";
        case TimeInForce::ImmediateOrCancel:
            return "ImmediateOrCancel";
    }
    return "unknown";
}

constexpr std::string_view to_string(const AddOrderStatus& status){
    switch(status){
        case AddOrderStatus::FullyFilled:
            return "FullyFilled";
        case AddOrderStatus::PartiallyFilled:
            return "PartiallyFilled";
        case AddOrderStatus::Resting:
            return "Resting";
        case AddOrderStatus::Failed:
            return "Failed";
        case AddOrderStatus::Rejected:
            return "Rejected";
    }
    return "unknown";
}

std::ostream& operator << (std::ostream& os, const TimeInForce& obj);
std::ostream& operator << (std::ostream& os, const Side& obj);
std::ostream& operator << (std::ostream& os, const OrderType& obj);
std::ostream& operator << (std::ostream& os, const AddOrderStatus& obj);