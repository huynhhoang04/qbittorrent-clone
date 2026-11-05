#pragma once

#include <bits/stdc++.h>

#include "database.hpp"
#include "download_manager.hpp"
#include "file_manager.hpp"
#include "metadata.hpp"
#include "state_manager.hpp"
#include "tracker_client.hpp"
#include "upload_server.hpp"

using namespace std;

struct peer_node_options {
    int max_active_downloads = 5;
    int max_active_uploads = 5;
    int pipeline_depth = 3;
    int io_timeout_ms = 4000;
    int chunk_delay_ms = 0;
    bool exit_after_download = false;
    vector<int> corrupt_chunks;
};

struct managed_torrent_status {
    string file_id;
    string file_name;
    string output_path;
    string mode; // seeding | downloading | completed | failed
    size_t file_size = 0;
    size_t chunk_size = 0;
    size_t num_chunks = 0;
    size_t completed_chunks = 0;
    double progress = 0.0;
};

struct peer_node_status {
    string peer_id;
    int port = 0;
    bool running = false;
    bool tracker_online = false;
    bool download_active = false;
    vector<managed_torrent_status> torrents;
    download_stats download;
    vector<peer_transfer_status> download_peers;
    vector<upload_peer_status> upload_peers;
    uint64_t uploaded_bytes = 0;
    uint64_t uploaded_chunks = 0;
    int active_upload_slots = 0;
    int peak_upload_slots = 0;
};

class PeerNode {
    private:
        string tracker_url;
        int port;
        string data_dir;
        string peer_id;
        peer_node_options options;

        unique_ptr<Database> db;
        unique_ptr<FileManager> file_manager;
        unique_ptr<StateManager> state_manager;
        unique_ptr<TrackerClient> tracker_client;
        unique_ptr<UploadServer> upload_server;
        unique_ptr<DownloadManager> download_manager;

        atomic<bool> is_running{false};
        atomic<bool> tracker_online{false};
        atomic<bool> download_active{false};
        atomic<bool> download_success{false};
        thread heartbeat_thread;
        thread download_thread;

        mutable mutex torrents_mtx;
        unordered_map<string, managed_torrent_status> torrents;

        unordered_set<string> active_files;
        mutex active_files_mtx;

        mutex announce_mtx;
        chrono::steady_clock::time_point last_announce{};

        void start_heartbeat();
        bool announce_file(const string& file_id, bool force = false);
        void remember_torrent(const string& file_id, const string& file_name,
                              const string& output_path, const string& mode,
                              size_t file_size, size_t chunk_size,
                              size_t num_chunks);
        void update_torrent_mode(const string& file_id, const string& mode);

    public:
        PeerNode(const string& tracker_url = "http://localhost:8080",
                 int port = 6881,
                 const string& data_dir = "./data/peer");
        ~PeerNode();

        void set_options(const peer_node_options& new_options);
        peer_node_options get_options() const;

        bool start();
        void stop();
        bool running() const { return is_running.load(); }

        bool seed_file(const string& file_path, size_t chunk_size = 256 * 1024);

        // Blocking download used by headless scripts.
        bool download_file(const string& torrent_path, const string& output_dir = "");

        // Non-blocking variant used by the TUI.
        bool start_download_async(const string& torrent_path,
                                  const string& output_dir = "");
        void wait_for_download();
        bool download_succeeded() const { return download_success.load(); }
        bool download_active_now() const { return download_active.load(); }

        void pause_download();
        void resume_download(bool resume = true);
        void request_stop_download();

        peer_node_status get_status() const;

        string get_peer_id() const { return peer_id; }
        int get_port() const { return port; }
        string get_data_dir() const { return data_dir; }
        bool exit_after_download() const { return options.exit_after_download; }
};
