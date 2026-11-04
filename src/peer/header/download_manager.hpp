#pragma once

#include <bits/stdc++.h>

#include "file_manager.hpp"
#include "metadata.hpp"
#include "protocol.hpp"
#include "state_manager.hpp"
#include "tracker_client.hpp"

using namespace std;

struct download_options {
    int max_active_downloads = 5;
    int pipeline_depth = 3;
    int max_attempts_per_chunk = 12;
    int connect_timeout_ms = 3000;
    int io_timeout_ms = 4000;
    int discovery_attempts = 3;
    int discovery_wait_ms = 500;
    int max_peer_failures = 3;
    bool verify_existing_chunks = true;
};

struct peer_transfer_status {
    string peer_id;
    string ip;
    int port = 0;
    bool connected = false;
    bool choked = true;
    uint64_t chunks_downloaded = 0;
    uint64_t bytes_downloaded = 0;
    string last_error;
};

struct download_stats {
    uint64_t chunks_verified = 0;
    uint64_t bytes_downloaded = 0;
    uint64_t duplicate_claims = 0;
    uint64_t retries = 0;
    uint64_t failed_requests = 0;
    uint64_t hash_mismatches = 0;
    int active_peers = 0;
    int peak_active_peers = 0;
};

class DownloadManager {
    public:
        using chunk_verified_callback =
            std::function<void(const string& file_id, int chunk_index)>;

        DownloadManager(FileManager* fm, StateManager* sm, TrackerClient* tc,
                        const string& my_peer_id = "");
        ~DownloadManager();

        // Downloads all missing chunks of one torrent. The call blocks until the
        // torrent is complete, cancelled, or the retry budget is exhausted.
        bool start_download(const torrent_info& meta, const string& file_path,
                            int num_threads = 0);

        void pause_download(const string& file_id);
        void resume_download(const string& file_id);
        bool is_paused(const string& file_id);
        void request_stop();

        void set_max_active_downloads(int limit);
        int get_max_active_downloads() const;
        void set_options(const download_options& options);
        download_options get_options() const;

        vector<peer_transfer_status> get_peer_status() const;
        download_stats get_stats() const;
        bool is_active() const;
        string last_error() const;

        void set_chunk_verified_callback(chunk_verified_callback callback);

        // Single-chunk helper retained from the M4 API.
        bool download_chunk_from_peer(const peer_info_response& peer,
                                      const string& file_id, int chunk_idx,
                                      size_t chunk_size, const string& expected_hash,
                                      const string& file_path);

    private:
        struct peer_session {
            peer_info_response peer;
            vector<bool> bitfield;
            thread worker;
            atomic<socket_t> sock{invalid_socket};
            atomic<bool> connected{false};
            atomic<bool> choked{true};
            atomic<bool> finished{false};
            atomic<uint64_t> chunks_downloaded{0};
            atomic<uint64_t> bytes_downloaded{0};
            string last_error;
        };

        bool run_scheduler();
        void start_available_sessions();
        void stop_sessions();
        bool discover_peers();
        bool verify_existing_chunks();
        bool verify_whole_file();
        void peer_worker(shared_ptr<peer_session> session);

        int claim_chunk(const shared_ptr<peer_session>& session);
        void complete_chunk(int chunk_index);
        void release_chunk(int chunk_index, const string& peer_id, bool retryable);
        void release_all(vector<int>& in_flight, const string& peer_id, bool retryable);

        size_t chunk_size_for(int chunk_index) const;
        string expected_hash(int chunk_index) const;
        bool work_remaining();
        bool stop_requested() const;
        bool is_paused_now() const;
        void wait_for_work(int milliseconds);

        void record_failure(const string& peer_id, const string& message);

        FileManager* file_manager;
        StateManager* state_manager;
        TrackerClient* tracker_client;
        string my_peer_id;

        mutable mutex mtx;
        condition_variable cv;

        download_options options;
        atomic<int> max_active_downloads{5};
        atomic<bool> stop_flag{false};
        atomic<bool> active_flag{false};
        atomic<bool> paused_flag{false};
        atomic<uint64_t> chunks_verified{0};
        atomic<uint64_t> bytes_downloaded{0};
        atomic<uint64_t> duplicate_claims{0};
        atomic<uint64_t> retry_count{0};
        atomic<uint64_t> failed_requests{0};
        atomic<uint64_t> hash_mismatches{0};
        atomic<int> active_peers{0};
        atomic<int> peak_active_peers{0};

        torrent_info current_meta;
        string current_file_id;
        string current_file_path;

        unordered_set<int> pending_chunks;
        unordered_set<int> claimed_chunks;
        unordered_set<int> failed_chunks;
        unordered_map<int, int> chunk_attempts;
        unordered_map<int, unordered_set<string>> blacklisted_peers;
        unordered_map<string, int> peer_failures;
        unordered_set<string> tried_peers;
        vector<peer_info_response> known_peers;
        unordered_map<string, shared_ptr<peer_session>> sessions;
        bool random_first_pending = false;
        bool download_started = false;

        string last_error_text;
        chunk_verified_callback verified_callback;
};
