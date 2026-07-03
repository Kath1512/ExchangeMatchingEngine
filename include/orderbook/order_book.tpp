#pragma once
#include "order_book.h"

template<typename EventSink>
OrderBook<EventSink>::OrderBook(EventSink& sink) : event_sink_(sink) {}

template<typename EventSink>
bool OrderBook<EventSink>::is_empty() const {
    return bids_.empty() && asks_.empty();
}

template<typename EventSink>
bool OrderBook<EventSink>::has_bids() const {
    return !bids_.empty();
}

template<typename EventSink>
bool OrderBook<EventSink>::has_asks() const {
    return !asks_.empty();
}

template<typename EventSink>
Price OrderBook<EventSink>::best_bid() const {
    if(!has_bids()) throw std::runtime_error("Bids are empty");
    return bids_.begin()->second.get_price();
}

template<typename EventSink>
Price OrderBook<EventSink>::best_ask() const {
    if(!has_asks()) throw std::runtime_error("Asks are empty");
    return asks_.begin()->second.get_price();
}

template<typename EventSink>
const TradeHistory& OrderBook<EventSink>::get_trades() const {
    return trades_;
}

template<typename EventSink>
bool OrderBook<EventSink>::has_level(Side side, Price price) const {
    if (side == Side::Buy) return bids_.count(price) > 0;
    return asks_.count(price) > 0;
}

template<typename EventSink>
Quantity OrderBook<EventSink>::level_quantity(Side side, Price price) const {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        return it != bids_.end() ? it->second.get_total_quantity() : 0;
    }
    auto it = asks_.find(price);
    return it != asks_.end() ? it->second.get_total_quantity() : 0;
}

template<typename EventSink>
std::size_t OrderBook<EventSink>::level_order_count(Side side, Price price) const {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        return it != bids_.end() ? it->second.get_order_count() : 0;
    }
    auto it = asks_.find(price);
    return it != asks_.end() ? it->second.get_order_count() : 0;
}

template<typename EventSink>
std::size_t OrderBook<EventSink>::bid_level_count() const { return bids_.size(); }

template<typename EventSink>
std::size_t OrderBook<EventSink>::ask_level_count() const { return asks_.size(); }

template<typename EventSink>
bool OrderBook<EventSink>::can_fully_fill(const Order& order) const {
    if(order.is_buy()) return can_fully_fill_against_side(asks_, order, cmp_buy);
    return can_fully_fill_against_side(bids_, order, cmp_sell);
}

template<typename EventSink>
AddOrderResult OrderBook<EventSink>::add_limit_order(Order&& order){
    const TimeInForce tif = order.get_time_in_force();

    if(tif == TimeInForce::FillOrKill && !can_fully_fill(order)){
        return AddOrderResult{
            .status_ = AddOrderStatus::Rejected,
            .execution_quantity_ = 0,
            .remaining_quantity_ = order.get_remaining_quantity()
        };
    }

    Quantity execution_quantity;
    if(order.is_buy()) execution_quantity = match_buy(order);
    else               execution_quantity = match_sell(order);

    Quantity remaining_quantity = order.get_remaining_quantity();

    if(tif == TimeInForce::GoodTillCancel){
        if(order.is_buy()){
            return AddOrderResult::from_match(
                add_order_to_side(bids_, std::move(order)),
                order.get_order_type(), execution_quantity, remaining_quantity
            );
        }
        return AddOrderResult::from_match(
            add_order_to_side(asks_, std::move(order)),
            order.get_order_type(), execution_quantity, remaining_quantity
        );
    }

    return AddOrderResult::from_match(order.get_order_type(), execution_quantity, remaining_quantity);
}

template<typename EventSink>
AddOrderResult OrderBook<EventSink>::add_market_order(Order&& order){
    Quantity execution_quantity;
    if(order.is_buy()) execution_quantity = match_market_order_against_side(asks_, order);
    else               execution_quantity = match_market_order_against_side(bids_, order);
    return AddOrderResult::from_match(order.get_order_type(), execution_quantity, order.get_remaining_quantity());
}

template<typename EventSink>
bool OrderBook<EventSink>::remove_look_up(OrderId order_id){
    return order_look_up_.erase(order_id);
}

template<typename EventSink>
AddOrderResult OrderBook<EventSink>::add_order(Order order){
    if(order.is_limit_order()) return add_limit_order(std::move(order));
    return add_market_order(std::move(order));
}

template<typename EventSink>
Quantity OrderBook<EventSink>::match_buy(Order& buy_order){
    return match_limit_order_against_side(buy_order, asks_, cmp_buy);
}

template<typename EventSink>
Quantity OrderBook<EventSink>::match_sell(Order& sell_order){
    return match_limit_order_against_side(sell_order, bids_, cmp_sell);
}

template<typename EventSink>
Quantity OrderBook<EventSink>::execute_trade(Order& incoming_order, PriceLevel& counterpart_level){
    Quantity execution_quantity = std::min(
        incoming_order.get_remaining_quantity(),
        counterpart_level.front_order().get_remaining_quantity()
    );

    Trade trade = Trade::from_match(incoming_order, counterpart_level.front_order());

    add_event(TradeExecuted{
        .buyer_id        = trade.get_buyer().order_id_,
        .seller_id       = trade.get_seller().order_id_,
        .price           = trade.get_execution_price(),
        .quantity        = trade.get_execution_quantity(),
        .aggressive_side = trade.get_aggressor_side()
    });

    trades_.push_back(trade);

    bool getFilled = (counterpart_level.front_order().get_remaining_quantity() == execution_quantity);
    OrderId front_order_id = counterpart_level.front_order().get_order_id();

    incoming_order.fill_order(execution_quantity);
    counterpart_level.fill_front_order(execution_quantity);

    if(getFilled) remove_look_up(front_order_id);

    return execution_quantity;
}

template<typename EventSink>
bool OrderBook<EventSink>::cancel_order(OrderId order_id){
    auto it = order_look_up_.find(order_id);
    if(it == order_look_up_.end()) return false;

    OrderLocation order_location = it->second;
    const Side& side = order_location.side_;
    order_look_up_.erase(it);

    if(side == Side::Buy) return cancel_order_side(bids_, std::move(order_location));
    return cancel_order_side(asks_, std::move(order_location));
}

template<typename EventSink>
MaybeOrderRef OrderBook<EventSink>::find_order(OrderId order_id) const {
    const auto it = order_look_up_.find(order_id);
    if(it == order_look_up_.end()) return std::nullopt;
    return *it->second.order_it_;
}

template<typename EventSink>
bool OrderBook<EventSink>::modify_order(OrderId order_id, Price new_price, Quantity new_quantity){
    const auto& maybe_old_order = find_order(order_id);
    if(!maybe_old_order.has_value()) return false;

    const Order& old_order = maybe_old_order.value().get();
    Order new_order = Order::from_replacement(old_order, new_price, new_quantity);

    cancel_order(order_id);

    AddOrderResult res = add_order(std::move(new_order));
    return res.status_ != AddOrderStatus::Failed;
}

template<typename EventSink>
void OrderBook<EventSink>::process(const AddOrder& add_order_msg){
    AddOrderResult res = add_order(Order(add_order_msg));
    OrderId id = add_order_msg.order_id;

    if(res.status_ == AddOrderStatus::Failed || res.status_ == AddOrderStatus::Rejected){
        add_event(OrderRejected{ .order_id = id, .reason = RejectReason::NoLiquidity });
        return;
    }
    add_event(OrderAccepted{ .order_id = id });
}

template<typename EventSink>
void OrderBook<EventSink>::process(const ModifyOrder& modify_order_msg){
    auto [id, new_price, new_quantity] = modify_order_msg;
    bool modified = modify_order(modify_order_msg.id, modify_order_msg.new_price, modify_order_msg.new_quantity);

    if(!modified){
        add_event(ModifyRejected{ .order_id = id, .reason = RejectReason::InvalidOrderId });
        return;
    }
    add_event(OrderModified{ .order_id = id, .new_price = new_price, .new_quantity = new_quantity });
}

template<typename EventSink>
void OrderBook<EventSink>::process(const CancelOrder& cancel_order_msg){
    bool cancelled = cancel_order(cancel_order_msg.id);
    OrderId id = cancel_order_msg.id;

    if(!cancelled){
        add_event(OrderRejected{ .order_id = id, .reason = RejectReason::InvalidOrderId });
        return;
    }
    add_event(OrderCanceled{ .order_id = id });
}

template<typename EventSink>
void OrderBook<EventSink>::process_message(const Message& msg){
    std::visit([this](auto&& action){ this->process(action); }, msg);
}

template<typename EventSink>
bool OrderBook<EventSink>::add_event(Event event){
    return event_sink_.push(std::move(event));
}

template<typename EventSink>
std::string OrderBook<EventSink>::to_string() const {
    std::string output = "================ ORDER BOOK ================\n";
    output += "Asks (Sellers):\n";
    output += levels_to_string(asks_);
    output += "Bids (Buyers):\n";
    output += levels_to_string(bids_);
    output += "============================================\n";
    return output;
}

template<typename EventSink>
std::string OrderBook<EventSink>::trades_to_string() const {
    std::string output = "";
    for(const Trade& trade : trades_) output += trade.to_string();
    return output;
}

template<typename ES>
std::ostream& operator<<(std::ostream& os, const OrderBook<ES>& book) {
    os << book.to_string();
    return os;
}
