#include "networking/msg_parser.h"
#include <atomic>
#include <vector>
// ── Helpers ───────────────────────────────────────────────────────────────────

static std::atomic<SequenceNumber> next_seq{1};

template<typename WireT, typename MsgT>
static MaybeMessage read_payload(ClientState& state) {
    std::array<uint8_t, sizeof(WireT)> buf;
    auto got = state.read(buf.data(), sizeof(WireT));

    if (got == -1) return std::nullopt;

    MsgT msg;
    if (!deserialise(buf.data(), msg)) return std::nullopt;

    return msg;
}

static void stamp_message(MaybeMessage& m, int connection_id) {
    SequenceNumber seq = next_seq.fetch_add(1, std::memory_order_relaxed);
    std::visit(
        [connection_id, seq](auto& msg) {
            msg.connection_id = connection_id;
            msg.seq = seq;
        },
        m.value()
    );
}

// ── Parser loop ───────────────────────────────────────────────────────────────

// void run_parser(int fd, MessageSink& sink, AtomicBool& running, int connection_id) {
//     while (running) {
//         OrderMsgHeader header;
//         auto got = recv_all(fd, reinterpret_cast<uint8_t*>(&header), sizeof(OrderMsgHeader));

//         if (got == -1) {
//             std::cerr << "Error\n";
//             break;
//         }
//         if (got == 0) {
//             std::cerr << "Connection closed\n";
//             running = false;
//             break;
//         }

//         auto [type, payload_len] = header;

//         MaybeMessage m;
//         switch (type) {
//             case OrderMsgType::AddOrder:
//                 m = recv_payload<WireAddOrder, AddOrder>(fd);
//                 break;
//             case OrderMsgType::CancelOrder:
//                 m = recv_payload<WireCancelOrder, CancelOrder>(fd);
//                 break;
//             case OrderMsgType::ModifyOrder:
//                 m = recv_payload<WireModifyOrder, ModifyOrder>(fd);
//                 break;
//             default:
//                 std::cerr << "Unknown message type\n";
//                 break;
//         }

//         if (m) {
//             stamp_message(m, connection_id);
//             sink.push(std::move(m.value()));
//         }
//     }
// }

static bool parse_message(
    ClientState& state,
    MessageSink& sink,
    int connection_id
) {
        OrderMsgHeader header;
        if(state.parse_state == ParseState::ReadingHeader){
            auto got = state.read(reinterpret_cast<BytePointer>(&header), sizeof(OrderMsgHeader));
            if (got == -1) {
                return false;
            }
            state.parse_state = ParseState::ReadingPayload;
            state.pending_header = header;
        }
        else{
            header = state.pending_header;
        }
        
        auto [type, payload_len] = header;

        MaybeMessage m;
        switch (type) {
            case OrderMsgType::AddOrder:
                m = read_payload<WireAddOrder, AddOrder>(state);
                break;
            case OrderMsgType::CancelOrder:
                m = read_payload<WireCancelOrder, CancelOrder>(state);
                break;
            case OrderMsgType::ModifyOrder:
                m = read_payload<WireModifyOrder, ModifyOrder>(state);
                break;
            default: {
                std::vector<Byte> discard(payload_len);
                auto got = state.read(discard.data(), payload_len);
                if (got == -1) return false;

                std::cerr << std::format("Unknown message type from connection {} — skipped {} bytes\n", connection_id, payload_len);
                state.parse_state = ParseState::ReadingHeader;
                return true;
            }
        }

        if(!m) return false;

        stamp_message(m, connection_id);
        sink.push(std::move(m.value()));
        state.parse_state = ParseState::ReadingHeader;
        return true;
}



int parse_ready_client(
    ClientState& state,
    MessageSink& sink,
    int connection_id
){
    int msg_parsed = 0;
    while(msg_parsed < MSG_LIMIT_PER_CONN){
        if(parse_message(state, sink, connection_id)){
            msg_parsed++;
        }
        else{
            break;
        }
    }

    return msg_parsed;
}
