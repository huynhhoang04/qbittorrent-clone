#include "protocol.hpp"

#include <cstring>
#include <stdexcept>

using namespace std;

vector<uint8_t> wire_message::serialize() const {
    vector<uint8_t> result;

    // length covers the message id plus the payload. A zero length frame is a
    // keep-alive and carries no id byte.
    uint32_t msg_length = 1 + static_cast<uint32_t>(payload.size());
    if (id == message_type::KEEPALIVE && payload.empty()) {
        msg_length = 0;
    }

    const uint32_t net_len = htonl(msg_length);
    result.resize(sizeof(net_len));
    memcpy(result.data(), &net_len, sizeof(net_len));

    if (msg_length > 0) {
        result.push_back(static_cast<uint8_t>(id));
        result.insert(result.end(), payload.begin(), payload.end());
    }

    return result;
}

wire_message wire_message::deserialize(const vector<uint8_t>& data) {
    if (data.size() < 4) {
        throw invalid_argument("Data too small to contain length prefix");
    }

    wire_message msg;
    uint32_t net_len = 0;
    memcpy(&net_len, data.data(), 4);
    msg.length = ntohl(net_len);

    if (msg.length > MAX_WIRE_MESSAGE_SIZE) {
        throw invalid_argument("Message exceeds maximum allowed size");
    }
    if (data.size() < 4 + static_cast<size_t>(msg.length)) {
        throw invalid_argument("Incomplete message data");
    }

    if (msg.length == 0) {
        msg.id = message_type::KEEPALIVE;
        return msg;
    }

    msg.id = static_cast<message_type>(data[4]);
    if (msg.length > 1) {
        msg.payload.assign(data.begin() + 5, data.begin() + 4 + msg.length);
    }

    return msg;
}

vector<uint8_t> encode_handshake(const handshake_info& info) {
    if (info.peer_id.size() > MAX_PEER_ID_LENGTH ||
        info.file_id.size() > MAX_FILE_ID_LENGTH) {
        throw invalid_argument("Handshake field exceeds maximum length");
    }

    vector<uint8_t> payload;
    const uint16_t peer_length = static_cast<uint16_t>(info.peer_id.size());
    const uint16_t net_peer_length = htons(peer_length);
    payload.resize(sizeof(net_peer_length));
    memcpy(payload.data(), &net_peer_length, sizeof(net_peer_length));
    payload.insert(payload.end(), info.peer_id.begin(), info.peer_id.end());
    payload.insert(payload.end(), info.file_id.begin(), info.file_id.end());
    return payload;
}

bool decode_handshake(const vector<uint8_t>& payload, handshake_info& out) {
    if (payload.size() < 2) {
        return false;
    }

    uint16_t net_peer_length = 0;
    memcpy(&net_peer_length, payload.data(), sizeof(net_peer_length));
    const size_t peer_length = ntohs(net_peer_length);
    if (peer_length > MAX_PEER_ID_LENGTH ||
        payload.size() < 2 + peer_length) {
        return false;
    }

    const size_t file_length = payload.size() - 2 - peer_length;
    if (file_length == 0 || file_length > MAX_FILE_ID_LENGTH) {
        return false;
    }

    out.peer_id.assign(reinterpret_cast<const char*>(payload.data() + 2), peer_length);
    out.file_id.assign(
        reinterpret_cast<const char*>(payload.data() + 2 + peer_length), file_length);
    return true;
}

bool send_wire_message(socket_t socket, const wire_message& message) {
    if (message.payload.size() + 1 > MAX_WIRE_MESSAGE_SIZE) {
        return false;
    }
    const vector<uint8_t> encoded = message.serialize();
    return net_send_all(socket, encoded.data(), encoded.size());
}

bool recv_wire_message(socket_t socket, wire_message& message, string& error) {
    uint32_t net_length = 0;
    if (!net_recv_exact(socket, &net_length, sizeof(net_length))) {
        error = "connection closed while reading frame length";
        return false;
    }

    const uint32_t length = ntohl(net_length);
    if (length > MAX_WIRE_MESSAGE_SIZE) {
        error = "frame length " + to_string(length) + " exceeds protocol maximum";
        return false;
    }

    message.length = length;
    message.payload.clear();

    if (length == 0) {
        message.id = message_type::KEEPALIVE;
        return true;
    }

    vector<uint8_t> body(length);
    if (!net_recv_exact(socket, body.data(), body.size())) {
        error = "connection closed while reading frame body";
        return false;
    }

    message.id = static_cast<message_type>(body[0]);
    if (length > 1) {
        message.payload.assign(body.begin() + 1, body.end());
    }
    return true;
}

const char* message_type_name(message_type type) {
    switch (type) {
        case message_type::HANDSHAKE: return "HANDSHAKE";
        case message_type::BITFIELD: return "BITFIELD";
        case message_type::REQUEST: return "REQUEST";
        case message_type::PIECE: return "PIECE";
        case message_type::CANCEL: return "CANCEL";
        case message_type::KEEPALIVE: return "KEEPALIVE";
        case message_type::INTERESTED: return "INTERESTED";
        case message_type::NOT_INTERESTED: return "NOT_INTERESTED";
        case message_type::CHOKE: return "CHOKE";
        case message_type::UNCHOKE: return "UNCHOKE";
    }
    return "UNKNOWN";
}
