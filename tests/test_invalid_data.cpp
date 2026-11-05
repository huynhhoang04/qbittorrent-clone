#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "database.hpp"
#include "file_manager.hpp"
#include "logger.hpp"
#include "net.hpp"
#include "protocol.hpp"
#include "state_manager.hpp"
#include "test_helpers.hpp"
#include "upload_server.hpp"

using namespace std;

namespace {

// A connected loopback TCP pair created through the same socket layer the
// application uses, so framing is exercised over a real byte stream rather
// than a mock.
class SocketPair {
    public:
        bool open(int timeout_ms = 3000) {
            if (!net_init()) {
                return false;
            }
            const int port = testutil::reserve_port();
            if (port <= 0) {
                return false;
            }

            listener = ::socket(AF_INET, SOCK_STREAM, 0);
            if (!net_valid(listener)) {
                return false;
            }
            const int reuse = 1;
            setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                       reinterpret_cast<const char*>(&reuse), sizeof(reuse));

            struct sockaddr_in address {};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = htons(static_cast<unsigned short>(port));
            if (bind(listener, reinterpret_cast<sockaddr*>(&address),
                     sizeof(address)) != 0) {
                return false;
            }
            if (listen(listener, 1) != 0) {
                return false;
            }

            client = ::socket(AF_INET, SOCK_STREAM, 0);
            if (!net_valid(client)) {
                return false;
            }
            if (!net_connect_timeout(client,
                                     reinterpret_cast<sockaddr*>(&address),
                                     sizeof(address), timeout_ms)) {
                return false;
            }
            server = accept(listener, nullptr, nullptr);
            if (!net_valid(server)) {
                return false;
            }
            return net_set_timeouts(server, timeout_ms) &&
                   net_set_timeouts(client, timeout_ms);
        }

        ~SocketPair() {
            if (net_valid(client)) {
                net_close(client);
            }
            if (net_valid(server)) {
                net_close(server);
            }
            if (net_valid(listener)) {
                net_close(listener);
            }
        }

        socket_t server_side() const { return server; }
        socket_t client_side() const { return client; }

        // Hangs up the writer side once, so a later destructor pass cannot
        // close a descriptor some other owner has already been handed.
        void close_client() {
            if (net_valid(client)) {
                net_close(client);
                client = invalid_socket;
            }
        }

    private:
        socket_t listener = invalid_socket;
        socket_t client = invalid_socket;
        socket_t server = invalid_socket;
};

vector<uint8_t> framed(uint32_t declared_length, const vector<uint8_t>& body) {
    vector<uint8_t> raw;
    const uint32_t net_length = htonl(declared_length);
    raw.resize(sizeof(net_length));
    memcpy(raw.data(), &net_length, sizeof(net_length));
    raw.insert(raw.end(), body.begin(), body.end());
    return raw;
}

// Writes bytes straight to a socket, bypassing the framing helpers so the
// receiver sees deliberately broken input.
bool write_raw(socket_t socket, const vector<uint8_t>& bytes) {
    return net_send_all(socket, bytes.data(), bytes.size());
}

} // namespace

TEST(InvalidDataTest, DeserializeRejectsOversizedLengthBeforeAllocating) {
    const vector<uint8_t> raw = framed(MAX_WIRE_MESSAGE_SIZE + 1024, {});
    EXPECT_THROW(wire_message::deserialize(raw), invalid_argument);

    // A hostile length must not be trusted even when it is the largest value a
    // uint32 can hold.
    const vector<uint8_t> huge = framed(numeric_limits<uint32_t>::max(), {});
    EXPECT_THROW(wire_message::deserialize(huge), invalid_argument);
}

TEST(InvalidDataTest, DeserializeRejectsTruncatedAndShortFrames) {
    // The prefix promises more bytes than the buffer holds.
    const vector<uint8_t> body = {static_cast<uint8_t>(message_type::REQUEST),
                                  1, 2, 3, 4};
    const vector<uint8_t> truncated = framed(1024, body);
    EXPECT_THROW(wire_message::deserialize(truncated), invalid_argument);

    for (size_t size = 0; size < 4; ++size) {
        vector<uint8_t> short_frame(size, 0);
        EXPECT_THROW(wire_message::deserialize(short_frame), invalid_argument)
            << "size " << size;
    }
}

TEST(InvalidDataTest, DeserializeHandlesKeepAliveAndBounds) {
    const vector<uint8_t> keep_alive = {0, 0, 0, 0};
    const wire_message alive = wire_message::deserialize(keep_alive);
    EXPECT_EQ(alive.length, 0u);
    EXPECT_EQ(alive.id, message_type::KEEPALIVE);
    EXPECT_TRUE(alive.payload.empty());

    wire_message request;
    request.id = message_type::REQUEST;
    request.payload = {0, 0, 0, 7};
    const vector<uint8_t> encoded = request.serialize();
    const wire_message decoded = wire_message::deserialize(encoded);
    EXPECT_EQ(decoded.id, message_type::REQUEST);
    EXPECT_EQ(decoded.payload, request.payload);
}

TEST(InvalidDataTest, HandshakeDecodeRejectsMalformedPayloads) {
    handshake_info parsed;

    // Too short to carry the u16 length prefix.
    EXPECT_FALSE(decode_handshake({0x00}, parsed));
    EXPECT_FALSE(decode_handshake({}, parsed));

    // Peer id length that exceeds the protocol maximum.
    vector<uint8_t> oversized_peer_id(4, 0);
    oversized_peer_id[0] = 0xFF;
    oversized_peer_id[1] = 0xFF;
    EXPECT_FALSE(decode_handshake(oversized_peer_id, parsed));

    // Length prefix that runs past the end of the payload.
    vector<uint8_t> lies_about_length = {0x00, 0x10, 'a', 'b'};
    EXPECT_FALSE(decode_handshake(lies_about_length, parsed));

    // Empty file id: a handshake without a torrent is meaningless.
    const vector<uint8_t> no_file_id = {0x00, 0x04, 'p', 'e', 'e', 'r'};
    EXPECT_FALSE(decode_handshake(no_file_id, parsed));

    // The well-formed case still works, which is what makes the rejections
    // above meaningful.
    const vector<uint8_t> good = encode_handshake({"peer-1", "abc123"});
    EXPECT_TRUE(decode_handshake(good, parsed));
    EXPECT_EQ(parsed.peer_id, "peer-1");
    EXPECT_EQ(parsed.file_id, "abc123");
}

TEST(InvalidDataTest, HandshakeFieldsOverTheLimitAreRefused) {
    EXPECT_THROW(encode_handshake({string(MAX_PEER_ID_LENGTH + 1, 'x'), "file"}),
                 invalid_argument);
    EXPECT_THROW(encode_handshake({"peer", string(MAX_FILE_ID_LENGTH + 1, 'y')}),
                 invalid_argument);
}

TEST(InvalidDataTest, SendingAnOversizedMessageIsRefusedWithoutWriting) {
    SocketPair pair;
    ASSERT_TRUE(pair.open());

    wire_message message;
    message.id = message_type::PIECE;
    message.payload.assign(static_cast<size_t>(MAX_WIRE_MESSAGE_SIZE) + 1, 0x5A);

    EXPECT_FALSE(send_wire_message(pair.client_side(), message));

    // Nothing was put on the wire, so the reader must still see a clean,
    // well-formed frame afterwards.
    wire_message keep_alive;
    keep_alive.id = message_type::KEEPALIVE;
    keep_alive.payload.clear();
    ASSERT_TRUE(send_wire_message(pair.client_side(), keep_alive));

    wire_message received;
    string error;
    ASSERT_TRUE(recv_wire_message(pair.server_side(), received, error)) << error;
    EXPECT_EQ(received.id, message_type::KEEPALIVE);
    EXPECT_EQ(received.length, 0u);
}

TEST(InvalidDataTest, ReceiverRejectsOversizedFrameAndDoesNotHang) {
    SocketPair pair;
    ASSERT_TRUE(pair.open());

    const uint32_t hostile = numeric_limits<uint32_t>::max() - 1;
    ASSERT_TRUE(write_raw(pair.client_side(), framed(hostile, {})));

    wire_message received;
    string error;
    const auto started = chrono::steady_clock::now();
    EXPECT_FALSE(recv_wire_message(pair.server_side(), received, error));
    const auto elapsed =
        chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now() - started).count();

    EXPECT_NE(error.find("exceeds protocol maximum"), string::npos) << error;
    // The rejection must come from the length check, not from an I/O timeout.
    EXPECT_LT(elapsed, 2500) << "rejecting the frame took " << elapsed << "ms";
}

TEST(InvalidDataTest, ReceiverHandlesTruncatedBodyWithoutHang) {
    SocketPair pair;
    ASSERT_TRUE(pair.open());

    // Announce a 4 KiB body, send six bytes of it, then hang up mid-frame.
    const vector<uint8_t> partial =
        framed(4096, {1, 2, 3, 4, 5, 6});
    ASSERT_TRUE(write_raw(pair.client_side(), partial));
    // Hang up through the owner so the descriptor is closed exactly once;
    // closing it directly here would leave ~SocketPair to close a number the
    // OS may already have handed to another thread's live socket.
    pair.close_client();

    wire_message received;
    string error;
    EXPECT_FALSE(recv_wire_message(pair.server_side(), received, error));
    EXPECT_FALSE(error.empty()) << "expected a read error description";
}

TEST(InvalidDataTest, UploadServerRefusesNonHandshakeTrafficAndStaysUsable) {
    const string root = testutil::make_temp_dir("m6_invalid_data");
    const string payload_path = root + "/blob.bin";
    ASSERT_TRUE(testutil::write_pattern_file(payload_path, 128 * 1024));

    Database db(root + "/peer.db");
    db.init_schema();
    FileManager files;
    StateManager state(&db);
    const string file_id = "invalid-data-file";
    state.init_file_state(file_id, 2, true);
    state.mark_chunk_complete(file_id, 0);
    state.mark_chunk_complete(file_id, 1);

    UploadServer server(0, &files, &state, "hardened-peer");
    ASSERT_TRUE(server.start());
    server.register_file(file_id, payload_path, 64 * 1024, 2);

    auto connect = [&server]() {
        socket_t socket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (!net_valid(socket)) {
            return socket;
        }
        struct sockaddr_in address {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(static_cast<unsigned short>(server.get_port()));
        if (!net_connect_timeout(socket, reinterpret_cast<sockaddr*>(&address),
                                 sizeof(address), 3000)) {
            net_close(socket);
            return invalid_socket;
        }
        net_set_timeouts(socket, 6000);
        return socket;
    };

    // Traffic that is not a HANDSHAKE must end the connection instead of being
    // served or leaving a worker stuck.
    socket_t rude = connect();
    ASSERT_TRUE(net_valid(rude));
    wire_message bogus;
    bogus.id = message_type::REQUEST;
    bogus.payload = {0, 0, 0, 0};
    ASSERT_TRUE(send_wire_message(rude, bogus));
    string error;
    wire_message reply;
    EXPECT_FALSE(recv_wire_message(rude, reply, error));
    net_close(rude);

    // Random bytes that cannot be a frame must also be survivable.
    socket_t garbage = connect();
    ASSERT_TRUE(net_valid(garbage));
    ASSERT_TRUE(write_raw(garbage, vector<uint8_t>(64, 0x7F)));
    EXPECT_FALSE(recv_wire_message(garbage, reply, error));
    net_close(garbage);

    // A well-behaved client still gets served after both attacks, which proves
    // the server was not wedged by them.
    socket_t good = connect();
    ASSERT_TRUE(net_valid(good));
    wire_message handshake;
    handshake.id = message_type::HANDSHAKE;
    handshake.payload = encode_handshake({"curious-peer", file_id});
    ASSERT_TRUE(send_wire_message(good, handshake));

    ASSERT_TRUE(recv_wire_message(good, reply, error)) << error;
    EXPECT_EQ(reply.id, message_type::HANDSHAKE);
    ASSERT_TRUE(recv_wire_message(good, reply, error)) << error;
    EXPECT_EQ(reply.id, message_type::BITFIELD);

    wire_message interested;
    interested.id = message_type::INTERESTED;
    ASSERT_TRUE(send_wire_message(good, interested));
    ASSERT_TRUE(recv_wire_message(good, reply, error)) << error;
    EXPECT_EQ(reply.id, message_type::UNCHOKE);

    const uint32_t index = htonl(0);
    wire_message request;
    request.id = message_type::REQUEST;
    request.payload.assign(4, 0);
    memcpy(request.payload.data(), &index, sizeof(index));
    ASSERT_TRUE(send_wire_message(good, request));
    ASSERT_TRUE(recv_wire_message(good, reply, error)) << error;
    EXPECT_EQ(reply.id, message_type::PIECE);
    // A PIECE payload carries the 4-byte chunk index ahead of the data.
    EXPECT_EQ(reply.payload.size(), 4u + 64 * 1024u);
    net_close(good);

    server.stop();
    testutil::remove_quietly(root);
}

TEST(InvalidDataTest, OutOfRangeChunkRequestClosesConnectionWithoutSlotLeak) {
    const string root = testutil::make_temp_dir("m6_out_of_range");
    const string payload_path = root + "/blob.bin";
    ASSERT_TRUE(testutil::write_pattern_file(payload_path, 64 * 1024));

    Database db(root + "/peer.db");
    db.init_schema();
    FileManager files;
    StateManager state(&db);
    const string file_id = "range-file";
    state.init_file_state(file_id, 1, true);
    state.mark_chunk_complete(file_id, 0);

    UploadServer server(0, &files, &state, "strict-peer");
    ASSERT_TRUE(server.start());
    server.register_file(file_id, payload_path, 64 * 1024, 1);

    socket_t socket = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_TRUE(net_valid(socket));
    struct sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<unsigned short>(server.get_port()));
    ASSERT_TRUE(net_connect_timeout(socket, reinterpret_cast<sockaddr*>(&address),
                                    sizeof(address), 3000));
    net_set_timeouts(socket, 6000);

    wire_message handshake;
    handshake.id = message_type::HANDSHAKE;
    handshake.payload = encode_handshake({"greedy-peer", file_id});
    ASSERT_TRUE(send_wire_message(socket, handshake));

    wire_message reply;
    string error;
    ASSERT_TRUE(recv_wire_message(socket, reply, error)) << error;
    ASSERT_TRUE(recv_wire_message(socket, reply, error)) << error;

    wire_message interested;
    interested.id = message_type::INTERESTED;
    ASSERT_TRUE(send_wire_message(socket, interested));
    ASSERT_TRUE(recv_wire_message(socket, reply, error)) << error;
    ASSERT_EQ(reply.id, message_type::UNCHOKE);

    // Ask for a chunk index the torrent does not have.
    const uint32_t beyond = htonl(4000);
    wire_message request;
    request.id = message_type::REQUEST;
    request.payload.assign(4, 0);
    memcpy(request.payload.data(), &beyond, sizeof(beyond));
    ASSERT_TRUE(send_wire_message(socket, request));

    EXPECT_FALSE(recv_wire_message(socket, reply, error));
    net_close(socket);

    // The upload slot taken by that connection must come back.
    EXPECT_TRUE(testutil::wait_until(
        [&server]() { return server.active_upload_slots() == 0; }, 8000, 50));

    server.stop();
    testutil::remove_quietly(root);
}
