#include <cstdint>
#include <vector>
#include <stdexcept>
#include <nlohmann/detail/macro_scope.hpp>
#pragma once
#include <bits/stdc++.h>
#include <nlohmann/json.hpp>

using namespace std;

// Dành cho Peer-to-Tracker (HTTP REST JSON)
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

// Dành cho Peer-to-Peer Wire Protocol (Raw TCP)
enum class message_type : uint8_t {
    HANDSHAKE = 0x00,
    BITFIELD = 0x01,
    REQUEST = 0x02,
    PIECE = 0x03,
    CANCEL = 0x04,
    KEEPALIVE = 0x05
};

struct wire_message {
    uint32_t length;
    message_type id;
    vector<uint8_t> payload;

    vector<uint8_t> serialize() const;
    static wire_message deserialize(const vector<uint8_t>& data);
};

class Protocol {
    public:
        // Optional methods if needed
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(announce_request, peer_id, ip, port, file_id, bitfield);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(peer_info_response, peer_id, ip, port, bitfield);