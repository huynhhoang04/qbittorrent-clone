#include <gtest/gtest.h>
#include "protocol.hpp"

using namespace std;

TEST(ProtocolTest, SerializeDeserializeKeepAlive) {
    wire_message msg;
    msg.length = 0;
    msg.id = message_type::KEEPALIVE;
    msg.payload.clear();

    auto serialized = msg.serialize();
    ASSERT_EQ(serialized.size(), 4);

    auto deserialized = wire_message::deserialize(serialized);
    EXPECT_EQ(deserialized.length, 0);
    EXPECT_EQ(deserialized.id, message_type::KEEPALIVE);
    EXPECT_TRUE(deserialized.payload.empty());
}

TEST(ProtocolTest, SerializeDeserializeRequest) {
    wire_message msg;
    msg.length = 5;
    msg.id = message_type::REQUEST;
    msg.payload = {0x00, 0x00, 0x00, 0x01}; // Piece index 1

    auto serialized = msg.serialize();
    ASSERT_EQ(serialized.size(), 4 + 1 + 4);

    auto deserialized = wire_message::deserialize(serialized);
    EXPECT_EQ(deserialized.length, 5);
    EXPECT_EQ(deserialized.id, message_type::REQUEST);
    EXPECT_EQ(deserialized.payload, msg.payload);
}

TEST(ProtocolTest, DeserializeIncompleteData) {
    vector<uint8_t> data = {0x00, 0x00, 0x00, 0x05, 0x04}; // Missing payload
    EXPECT_THROW(wire_message::deserialize(data), invalid_argument);
}

TEST(ProtocolTest, JSONSerializeDeserialize) {
    announce_request req;
    req.peer_id = "peer1";
    req.ip = "127.0.0.1";
    req.port = 6881;
    req.file_id = "file1";
    req.bitfield = {true, false, true};

    nlohmann::json j = req;
    announce_request req_out = j.get<announce_request>();

    EXPECT_EQ(req.peer_id, req_out.peer_id);
    EXPECT_EQ(req.ip, req_out.ip);
    EXPECT_EQ(req.port, req_out.port);
    EXPECT_EQ(req.file_id, req_out.file_id);
    EXPECT_EQ(req.bitfield, req_out.bitfield);
}
