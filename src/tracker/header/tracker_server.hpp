#pragma once
#include <bits/stdc++.h>
#include <crow.h>
#include "protocol.hpp"
#include "peer_registry.hpp"

using namespace std;

void start_tracker_server(int port = 8080);
void stop_tracker_server();

class TrackerServer {
    private:
        crow::SimpleApp app;
        PeerRegistry registry;
        int port;
        atomic<bool> running{false};
        thread cleanup_thread;
        condition_variable cv;
        mutex cv_mtx;

    public:
        TrackerServer(int port = 8080);
        ~TrackerServer();

        void setup_routes();
        void start();
        void stop();

        PeerRegistry& get_registry();
        int get_port() const;
        bool is_running() const;
};
