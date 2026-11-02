#pragma once
#include <bits/stdc++.h>
#include "file_manager.hpp"
#include "state_manager.hpp"
#include "tracker_client.hpp"
#include "metadata.hpp"
#include "protocol.hpp"

using namespace std;

class DownloadManager {
    private:
        FileManager* file_manager;
        StateManager* state_manager;
        TrackerClient* tracker_client;
        string my_peer_id;

        unordered_set<string> paused_files;
        mutex mtx;

    public:
        DownloadManager(FileManager* fm, StateManager* sm, TrackerClient* tc, const string& my_peer_id = "");

        bool start_download(const torrent_info& meta, const string& file_path, int num_threads = 4);
        void pause_download(const string& file_id);
        void resume_download(const string& file_id);
        bool is_paused(const string& file_id);

        bool download_chunk_from_peer(const peer_info_response& peer, const string& file_id, int chunk_idx, size_t chunk_size, const string& expected_hash, const string& file_path);
};
