#include "protocol.hpp"
#include <arpa/inet.h>
#include <cstring>

using namespace std;

vector<uint8_t> wire_message::serialize() const {
    vector<uint8_t> result;
    
    // length = 1 byte for ID + payload size
    uint32_t msg_length = 1 + payload.size();
    if (id == message_type::KEEPALIVE && payload.empty()) {
        msg_length = 0;
    }
    
    // Network byte order (big-endian)
    uint32_t net_len = htonl(msg_length);
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
    uint32_t net_len;
    memcpy(&net_len, data.data(), 4);
    msg.length = ntohl(net_len);

    if (data.size() < 4 + msg.length) {
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
