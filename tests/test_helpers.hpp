#pragma once

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "net.hpp"
#include "sha256.hpp"

namespace testutil {

inline int reserve_port() {
    net_init();
    socket_t socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (!net_valid(socket)) {
        return 0;
    }

    struct sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        net_close(socket);
        return 0;
    }

    struct sockaddr_in bound {};
    socklen_t length = sizeof(bound);
    if (getsockname(socket, reinterpret_cast<sockaddr*>(&bound), &length) != 0) {
        net_close(socket);
        return 0;
    }
    const int port = ntohs(bound.sin_port);
    net_close(socket);
    return port;
}

inline std::string make_temp_dir(const std::string& tag) {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("qbittorrent_" + tag + "_" + std::to_string(nonce));
    std::filesystem::create_directories(directory);
    return directory.string();
}

inline bool write_pattern_file(const std::string& path, std::size_t size,
                               unsigned int seed = 0x9E3779B9u) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    std::vector<char> buffer(64 * 1024);
    std::size_t written = 0;
    unsigned int counter = seed;
    while (written < size) {
        const std::size_t count = std::min(buffer.size(), size - written);
        for (std::size_t i = 0; i < count; ++i) {
            counter = counter * 1664525u + 1013904223u;
            buffer[i] = static_cast<char>((counter >> 16) & 0xFF);
        }
        file.write(buffer.data(), static_cast<std::streamsize>(count));
        written += count;
    }
    return file.good();
}

inline std::string file_sha256(const std::string& path) {
    SHA256 sha;
    return sha.hash_file(path);
}

template <typename Predicate>
inline bool wait_until(Predicate predicate, int timeout_ms, int step_ms = 25) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(step_ms));
    }
    return predicate();
}

inline bool remove_quietly(const std::string& path) {
    std::error_code error;
    std::filesystem::remove_all(path, error);
    return !error;
}

} // namespace testutil
