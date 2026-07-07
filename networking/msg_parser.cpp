#include "networking/msg_parser.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

template<typename WireT, typename MsgT>
static MaybeMessage recv_payload(int fd) {
    std::array<uint8_t, sizeof(WireT)> buf;

    auto got = recv_all(fd, buf.data(), buf.size());
    if (got <= 0) return std::nullopt;

    MsgT msg;
    if (!deserialise(buf.data(), msg)) return std::nullopt;

    return msg;
}

static void stamp_connection(MaybeMessage& m, int connection_id) {
    std::visit(
        [connection_id](auto& msg) { msg.connection_id = connection_id; },
        m.value()
    );
}

// ── Parser loop ───────────────────────────────────────────────────────────────

void run_parser(int fd, MessageSink& sink, AtomicBool& running, int connection_id) {
    while (running) {
        OrderMsgHeader header;
        auto got = recv_all(fd, reinterpret_cast<uint8_t*>(&header), sizeof(OrderMsgHeader));

        if (got == -1) {
            std::cerr << "Error\n";
            break;
        }
        if (got == 0) {
            std::cerr << "Connection closed\n";
            running = false;
            break;
        }

        auto [type, payload_len] = header;

        MaybeMessage m;
        switch (type) {
            case OrderMsgType::AddOrder:
                m = recv_payload<WireAddOrder, AddOrder>(fd);
                break;
            case OrderMsgType::CancelOrder:
                m = recv_payload<WireCancelOrder, CancelOrder>(fd);
                break;
            case OrderMsgType::ModifyOrder:
                m = recv_payload<WireModifyOrder, ModifyOrder>(fd);
                break;
            default:
                std::cerr << "Unknown message type\n";
                break;
        }

        if (m) {
            stamp_connection(m, connection_id);
            sink.push(std::move(m.value()));
        }
    }
}
