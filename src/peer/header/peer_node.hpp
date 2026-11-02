#pragma once
#include <bits/stdc++.h>
#include "database.hpp"
#include "file_manager.hpp"
#include "state_manager.hpp"
#include "tracker_client.hpp"
#include "upload_server.hpp"
#include "download_manager.hpp"
#include "metadata.hpp"

using namespace std;

class PeerNode {
    private:
        string tracker_url;
        int port;
        string data_dir;
        string peer_id;

        unique_ptr<Database> db;
        unique_ptr<FileManager> file_manager;
        unique_ptr<StateManager> state_manager;
        unique_ptr<TrackerClient> tracker_client;
        unique_ptr<UploadServer> upload_server;
        unique_ptr<DownloadManager> download_manager;

        atomic<bool> is_running{false};
        thread heartbeat_thread;
        unordered_set<string> active_files;
        mutex active_files_mtx;

        void start_heartbeat();

    public:
        PeerNode(const string& tracker_url = "http://localhost:8080", 
                 int port = 6881, 
                 const string& data_dir = "./data/peer");
        ~PeerNode();

        bool start();
        void stop();

        bool seed_file(const string& file_path, size_t chunk_size = 256 * 1024);
        bool download_file(const string& torrent_path, const string& output_dir = "");

        string get_peer_id() const { return peer_id; }
        int get_port() const { return port; }
        bool running() const { return is_running.load(); }
};
