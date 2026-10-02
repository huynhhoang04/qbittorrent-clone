#include <gtest/gtest.h>
#include "file_manager.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include "logger.hpp"

TEST(FileManagerTest, CreateWriteRead) {
    Logger log;
    log.info("Chạy bài test FileManagerTest.CreateWriteRead...");
    FileManager fm;
    std::string file_path = "test_download.bin";
    
    if (std::filesystem::exists(file_path)) {
        std::filesystem::remove(file_path);
    }
    
    // 1. Tạo file rỗng
    EXPECT_TRUE(fm.create_empty_file(file_path, 10));
    EXPECT_EQ(std::filesystem::file_size(file_path), 10);
    
    // 2. Ghi chunk (chunk size = 4, index = 1 -> offset = 4)
    std::vector<char> data = {'A', 'B', 'C', 'D'};
    EXPECT_TRUE(fm.write_chunk(file_path, 4, 1, data));
    
    // 3. Đọc lại chunk
    std::vector<char> read_data = fm.read_chunk(file_path, 1, 4);
    ASSERT_EQ(read_data.size(), 4);
    EXPECT_EQ(read_data[0], 'A');
    EXPECT_EQ(read_data[1], 'B');
    EXPECT_EQ(read_data[2], 'C');
    EXPECT_EQ(read_data[3], 'D');
    
    std::filesystem::remove(file_path);
}
