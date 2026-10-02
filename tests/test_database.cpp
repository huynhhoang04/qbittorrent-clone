#include <gtest/gtest.h>
#include "database.hpp"
#include <cstdio>

using namespace std;

class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        db = make_unique<Database>(":memory:");
        db->init_schema();
    }

    void TearDown() override {
        db.reset();
    }

    unique_ptr<Database> db;
};

TEST_F(DatabaseTest, InsertAndGetTorrent) {
    torrent_record tr;
    tr.file_id = "file_123";
    tr.file_name = "test.txt";
    tr.file_size = 1024;
    tr.chunk_size = 256;
    tr.num_chunks = 4;
    tr.file_hash = "hash123";
    tr.status = "downloading";
    tr.metadata_path = "/tmp/test.torrent.json";
    tr.output_path = "/tmp/test.txt";
    tr.uploaded = 0;
    tr.downloaded = 0;

    db->upsert_torrent(tr);

    auto torrents = db->get_all_torrents();
    ASSERT_EQ(torrents.size(), 1);
    EXPECT_EQ(torrents[0].file_id, "file_123");
    EXPECT_EQ(torrents[0].file_name, "test.txt");
    EXPECT_EQ(torrents[0].num_chunks, 4);
    EXPECT_EQ(torrents[0].status, "downloading");
}

TEST_F(DatabaseTest, ChunkOperations) {
    torrent_record tr;
    tr.file_id = "file_456";
    tr.file_name = "data.bin";
    tr.file_size = 2048;
    tr.chunk_size = 512;
    tr.num_chunks = 4;
    tr.file_hash = "hash456";
    tr.status = "downloading";
    tr.metadata_path = "/tmp/data.torrent.json";
    tr.output_path = "/tmp/data.bin";
    tr.uploaded = 0;
    tr.downloaded = 0;
    
    db->upsert_torrent(tr);

    chunk_record cr{"file_456", 0, "chunkhash1", "missing"};
    db->upsert_chunk(cr);

    auto chunks = db->get_chunks("file_456");
    ASSERT_EQ(chunks.size(), 1);
    EXPECT_EQ(chunks[0].status, "missing");

    db->update_chunk_status("file_456", 0, "verified");
    chunks = db->get_chunks("file_456");
    EXPECT_EQ(chunks[0].status, "verified");
}

TEST_F(DatabaseTest, PeerHistoryOps) {
    torrent_record tr;
    tr.file_id = "file_789";
    tr.file_name = "movie.mp4";
    tr.file_size = 4096;
    tr.chunk_size = 1024;
    tr.num_chunks = 4;
    tr.file_hash = "hash789";
    tr.status = "seeding";
    tr.metadata_path = "/tmp/movie.torrent.json";
    tr.output_path = "/tmp/movie.mp4";
    tr.uploaded = 1024;
    tr.downloaded = 4096;
    
    db->upsert_torrent(tr);

    peer_history_record ph{"peer1", "127.0.0.1", 5000, "file_789", ""};
    db->upsert_peer_history(ph);

    auto peers = db->get_peers_for_file("file_789");
    ASSERT_EQ(peers.size(), 1);
    EXPECT_EQ(peers[0].peer_id, "peer1");
    EXPECT_EQ(peers[0].ip, "127.0.0.1");
    EXPECT_EQ(peers[0].port, 5000);
}
