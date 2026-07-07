#include "networking/msg_sender.h"


void run_sender(int fd, const Message& order_msg){
    bool ok = std::visit(overloaded{
        [fd](const AddOrder& msg) {
            return send_msg<WireAddOrder, AddOrder>(fd, msg);
        },
        [fd](const ModifyOrder& msg) {
            return send_msg<WireModifyOrder, ModifyOrder>(fd, msg);
        },
        [fd](const CancelOrder& msg) {
            return send_msg<WireCancelOrder, CancelOrder>(fd, msg);
        }
    }, order_msg);
    
    if(!ok) return;
}