#pragma once

#include <bits/stdc++.h>

#include "file_manager.hpp"
#include "net.hpp"
#include "protocol.hpp"
#include "state_manager.hpp"

using namespace std;

struct upload_options {
    int max_active_uploads = 5;
    int worker_threads = 8;
    int io_timeout_ms = 5000;
    // Must stay below the requester's io_timeout_ms: while it waits for a slot
    // the server sends nothing, so a longer wait makes a healthy but saturated
    // peer look like a dead one and burns the requester's failure budget.
    int choke_wait_ms = 2000;
    size_t queue_capacity = 64;
    // Artificial per-chunk delay. It defaults to zero and is only used by the
    // churn tests and demos so a peer can be killed while it is uploading.
    int chunk_delay_ms = 0;
};

struct upload_peer_status {
    string peer_id;
    string ip;
    int port = 0;
    bool choked = true;
    uint64_t chunks_uploaded = 0;
    uint64_t bytes_uploaded = 0;
};

class UploadServer {
    public:
        UploadServer(int listen_port, FileManager* fm, StateManager* sm = nullptr,
                     const string& peer_id = "peer-node");
        ~UploadServer();

        void register_file(const string& file_id, const string& file_path,
                           size_t chunk_size, size_t num_chunks = 0);
        bool start();
        void stop();
        bool running() const;
        int get_port() const;

        void set_upload_options(const upload_options& options);
        upload_options get_upload_options() const;
        void set_max_active_uploads(int limit);
        int get_max_active_uploads() const;

        vector<upload_peer_status> get_peer_status() const;
        uint64_t total_bytes_uploaded() const;
        uint64_t total_chunks_uploaded() const;
        int active_upload_slots() const;
        int peak_upload_slots() const;

        // Fault injection used by the corrupted-chunk demo and tests.
        void set_corrupt_chunks(const vector<int>& chunks);
        void clear_corrupt_chunks();

    private:
        struct file_entry {
            string path;
            size_t chunk_size = 0;
            size_t num_chunks = 0;
            size_t file_size = 0;
        };

        void accept_loop();
        void worker_loop();
        void handle_client(socket_t client_socket);
        bool acquire_upload_slot(int timeout_ms);
        void release_upload_slot();
        void track_socket(socket_t socket, bool add);

        int port;
        socket_t listen_socket = invalid_socket;
        atomic<bool> is_running{false};
        FileManager* file_manager;
        StateManager* state_manager;
        string my_peer_id;

        upload_options options;
        mutable mutex files_mtx;
        unordered_map<string, file_entry> files;
        unordered_set<int> corrupt_chunks;
        atomic<uint64_t> bytes_uploaded{0};
        atomic<uint64_t> chunks_uploaded{0};

        thread accept_thread;
        vector<thread> workers;
        deque<socket_t> pending_sockets;
        mutex queue_mtx;
        condition_variable queue_cv;

        mutable mutex slot_mtx;
        condition_variable slot_cv;
        int active_slots = 0;
        int peak_slots = 0;

        mutable mutex sessions_mtx;
        unordered_map<string, upload_peer_status> session_status;

        mutex sockets_mtx;
        unordered_set<socket_t> active_sockets;
};
