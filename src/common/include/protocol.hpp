#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net.hpp"
#include <nlohmann/json.hpp>

using namespace std;

// Peer-to-Tracker (HTTP REST JSON)
struct announce_request {
    string peer_id;
    string ip;
    int port;
    string file_id;
    vector<bool> bitfield;
};

struct peer_info_response {
    string peer_id;
    string ip;
    int port;
    vector<bool> bitfield;
};

// Peer-to-Peer wire protocol (raw TCP).
enum class message_type : uint8_t {
    HANDSHAKE = 0x00,
    BITFIELD = 0x01,
    REQUEST = 0x02,
    PIECE = 0x03,
    CANCEL = 0x04,
    KEEPALIVE = 0x05,
    INTERESTED = 0x06,
    NOT_INTERESTED = 0x07,
    CHOKE = 0x08,
    UNCHOKE = 0x09
};

// A frame larger than this is treated as a protocol violation and the
// connection is closed before any allocation happens.
inline constexpr uint32_t MAX_WIRE_MESSAGE_SIZE = 8u * 1024u * 1024u;
inline constexpr size_t MAX_PEER_ID_LENGTH = 128;
inline constexpr size_t MAX_FILE_ID_LENGTH = 128;

struct wire_message {
    uint32_t length = 0;
    message_type id = message_type::KEEPALIVE;
    vector<uint8_t> payload;

    vector<uint8_t> serialize() const;
    static wire_message deserialize(const vector<uint8_t>& data);
};

struct handshake_info {
    string peer_id;
    string file_id;
};

vector<uint8_t> encode_handshake(const handshake_info& info);
bool decode_handshake(const vector<uint8_t>& payload, handshake_info& out);

bool send_wire_message(socket_t socket, const wire_message& message);
bool recv_wire_message(socket_t socket, wire_message& message, string& error);

const char* message_type_name(message_type type);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(announce_request, peer_id, ip, port, file_id, bitfield);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(peer_info_response, peer_id, ip, port, bitfield);
