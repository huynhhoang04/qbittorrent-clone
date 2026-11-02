#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include "peer_registry.hpp"
#include "logger.hpp"

using namespace std;

TEST(PeerRegistryTest, AddAndQuery) {
    Logger log;
    log.info("Chạy bài test PeerRegistryTest.AddAndQuery...");
    
    PeerRegistry registry;
    peer_entry p1;
    p1.peer_id = "peer-01";
    p1.ip = "127.0.0.1";
    p1.port = 6881;
    p1.bitfield = {true, false, true};

    registry.add_or_update("file-hash-1", p1);

    auto peers = registry.get_peers("file-hash-1");
    ASSERT_EQ(peers.size(), 1);
    EXPECT_EQ(peers[0].peer_id, "peer-01");
    EXPECT_EQ(peers[0].ip, "127.0.0.1");
    EXPECT_EQ(peers[0].port, 6881);
    EXPECT_EQ(peers[0].bitfield, vector<bool>({true, false, true}));
}

TEST(PeerRegistryTest, UpdateLastSeen) {
    Logger log;
    log.info("Chạy bài test PeerRegistryTest.UpdateLastSeen...");

    PeerRegistry registry;
    peer_entry p1;
    p1.peer_id = "peer-01";
    p1.ip = "127.0.0.1";
    p1.port = 6881;
    p1.bitfield = {false, false};

    registry.add_or_update("file-hash-1", p1);

    // Cập nhật lại port và bitfield
    p1.port = 6882;
    p1.bitfield = {true, true};
    registry.add_or_update("file-hash-1", p1);

    auto peers = registry.get_peers("file-hash-1");
    ASSERT_EQ(peers.size(), 1); // Không bị nhân đôi
    EXPECT_EQ(peers[0].port, 6882);
    EXPECT_EQ(peers[0].bitfield, vector<bool>({true, true}));
}

TEST(PeerRegistryTest, TimeoutCleanup) {
    Logger log;
    log.info("Chạy bài test PeerRegistryTest.TimeoutCleanup...");

    PeerRegistry registry;
    peer_entry p1;
    p1.peer_id = "peer-01";
    p1.ip = "127.0.0.1";
    p1.port = 6881;
    // Giả lập thời gian trong quá khứ: cách hiện tại 50 giây (> timeout 45s)
    p1.last_seen = chrono::steady_clock::now() - chrono::seconds(50);
    registry.add_or_update("file-hash-1", p1);

    peer_entry p2;
    p2.peer_id = "peer-02";
    p2.ip = "127.0.0.2";
    p2.port = 6882;
    p2.last_seen = chrono::steady_clock::now(); // Vừa mới xuất hiện
    registry.add_or_update("file-hash-1", p2);

    ASSERT_EQ(registry.get_peers("file-hash-1").size(), 2);

    // Chạy dọn dẹp timeout 45s
    registry.cleanup_stale(45);

    auto peers = registry.get_peers("file-hash-1");
    ASSERT_EQ(peers.size(), 1);
    EXPECT_EQ(peers[0].peer_id, "peer-02");
}

TEST(PeerRegistryTest, MultiFileSwarm) {
    Logger log;
    log.info("Chạy bài test PeerRegistryTest.MultiFileSwarm...");

    PeerRegistry registry;
    peer_entry p1;
    p1.peer_id = "peer-01";
    p1.ip = "127.0.0.1";
    p1.port = 6881;

    peer_entry p2;
    p2.peer_id = "peer-02";
    p2.ip = "127.0.0.2";
    p2.port = 6882;

    registry.add_or_update("file-A", p1);
    registry.add_or_update("file-B", p2);

    auto peers_A = registry.get_peers("file-A");
    auto peers_B = registry.get_peers("file-B");

    ASSERT_EQ(peers_A.size(), 1);
    EXPECT_EQ(peers_A[0].peer_id, "peer-01");

    ASSERT_EQ(peers_B.size(), 1);
    EXPECT_EQ(peers_B[0].peer_id, "peer-02");

    // File không tồn tại
    EXPECT_TRUE(registry.get_peers("file-non-existent").empty());
    EXPECT_EQ(registry.total_peers(), 2);
}

TEST(PeerRegistryTest, ConcurrencyThreadSafety) {
    Logger log;
    log.info("Chạy bài test PeerRegistryTest.ConcurrencyThreadSafety...");

    PeerRegistry registry;
    const int num_threads = 10;
    const int ops_per_thread = 100;
    vector<thread> workers;

    for (int t = 0; t < num_threads; ++t) {
        workers.emplace_back([&registry, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                peer_entry p;
                p.peer_id = "peer-" + to_string(t) + "-" + to_string(i);
                p.ip = "10.0.0." + to_string(t);
                p.port = 6000 + i;
                p.bitfield = {true, false};

                string fid = "file-" + to_string(i % 5);
                registry.add_or_update(fid, p);
                auto res = registry.get_peers(fid);
                EXPECT_FALSE(res.empty());
            }
        });
    }

    for (auto& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }

    EXPECT_GT(registry.total_peers(), 0);
}
