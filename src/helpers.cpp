#include "helpers.h"
#include<string>

std::ostream& operator << (std::ostream& os, const TimeInForce& obj){
    os << std::string(to_string(obj));
    return os;
}
std::ostream& operator << (std::ostream& os, const Side& obj){
    os << std::string(to_string(obj));
    return os;
}