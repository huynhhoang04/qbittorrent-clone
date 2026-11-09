#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <crow.h>

class PeerNode;

class WebUiServer {
private:
    PeerNode& node;
    int port;
    crow::SimpleApp app;
    std::atomic<bool> is_running{false};
    std::thread server_thread;

    std::mutex log_mtx;
    std::deque<std::string> log_history;

    std::chrono::steady_clock::time_point last_sample;
    uint64_t last_download_bytes = 0;
    uint64_t last_upload_bytes = 0;
    double current_download_speed = 0.0;
    double current_upload_speed = 0.0;

    void setup_routes();
    void update_speed_samples();

public:
    WebUiServer(PeerNode& node, int port = 8081);
    ~WebUiServer();

    bool start();
    void stop();
    bool running() const { return is_running.load(); }
    int get_port() const { return port; }

    void add_log(const std::string& line);
};
