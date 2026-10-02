#include <gtest/gtest.h>
#include "metadata.hpp"
#include <fstream>
#include <filesystem>
#include <string>
#include "logger.hpp"

TEST(MetadataTest, GenerateAndLoad) {
    Logger log;
    log.info("Chạy bài test MetadataTest.GenerateAndLoad...");
    std::string dummy_file = "dummy_test.txt";
    std::ofstream out(dummy_file);
    out << "1234567890"; // 10 bytes
    out.close();

    std::string torrent_file = "dummy_test.torrent.json";
    
    // Tải chunk size = 4 bytes
    Metadata meta;
    bool success = meta.generate_torrent(dummy_file, 4, torrent_file, "http://127.0.0.1:8080");
    EXPECT_TRUE(success);

    torrent_info info = meta.load(torrent_file);
    EXPECT_EQ(info.file_name, "dummy_test.txt");
    EXPECT_EQ(info.file_size, 10);
    EXPECT_EQ(info.chunk_size, 4);
    EXPECT_EQ(info.num_chunks, 3); // 10 bytes / 4 = 2.5 -> ceil(2.5) = 3
    EXPECT_EQ(info.tracker_url, "http://127.0.0.1:8080");
    EXPECT_EQ(info.chunk_hashs.size(), 3);

    std::filesystem::remove(dummy_file);
    std::filesystem::remove(torrent_file);
}
