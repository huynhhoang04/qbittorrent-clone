#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "tracker_server.hpp"
#include "tracker_client.hpp"
#include "logger.hpp"

using namespace std;

TEST(TrackerServerIntegrationTest, EndToEndFlow) {
    Logger log;
    log.info("Chạy bài test TrackerServerIntegrationTest.EndToEndFlow...");

    int test_port = 8990;
    TrackerServer server(test_port);

    // Chạy server trên thread riêng
    thread server_thread([&server]() {
        server.start();
    });

    // Cho server 200ms để khởi động socket
    this_thread::sleep_for(chrono::milliseconds(200));

    TrackerClient client("http://127.0.0.1:" + to_string(test_port));

    // 1. Kiểm tra health check
    EXPECT_TRUE(client.health_check());

    // 2. Announce peer mới
    announce_request req;
    req.peer_id = "test-peer-99";
    req.ip = "127.0.0.1";
    req.port = 6888;
    req.file_id = "file-xyz-123";
    req.bitfield = {true, true, false, true};

    EXPECT_TRUE(client.announce(req));

    // 3. Query danh sách peer cho file-xyz-123
    auto peers = client.get_peers("file-xyz-123");
    ASSERT_EQ(peers.size(), 1);
    EXPECT_EQ(peers[0].peer_id, "test-peer-99");
    EXPECT_EQ(peers[0].port, 6888);
    EXPECT_EQ(peers[0].bitfield, vector<bool>({true, true, false, true}));

    // 4. Query file không tồn tại
    auto empty_peers = client.get_peers("unknown-file");
    EXPECT_TRUE(empty_peers.empty());

    // Dừng server
    server.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}
