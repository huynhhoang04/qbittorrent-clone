#include <gtest/gtest.h>
#include "sha256.hpp"
#include <string>
#include "logger.hpp"

TEST(SHA256Test, HashBuffer) {
    Logger log;
    log.info("Chạy bài test SHA256Test.HashBuffer...");
    SHA256 sha;
    std::string data = "hello world";
    std::string expected = "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9";
    EXPECT_EQ(sha.hash_buffer(data), expected);
}

TEST(SHA256Test, HashEmpty) {
    SHA256 sha;
    std::string data = "";
    std::string expected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    EXPECT_EQ(sha.hash_buffer(data), expected);
}
