#pragma once
#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "file_manager.hpp"
#include "protocol.hpp"
#include "state_manager.hpp"

using namespace std;

struct upload_session {
    int client_socket;
    string peer_id;
    bool is_choked;
};

class UploadServer {
    private:
        int server_fd;
        int port;
        atomic<bool> is_running{false};
        thread server_thread;
        FileManager* file_manager;
        StateManager* state_manager;
        string my_peer_id;

        unordered_map<string, string> file_paths;
        unordered_map<string, size_t> chunk_sizes;
        mutex files_mtx;

    public:
        UploadServer(int listen_port, FileManager* fm, StateManager* sm = nullptr, const string& peer_id = "peer-node");
        ~UploadServer();

        void register_file(const string& file_id, const string& file_path, size_t chunk_size);
        bool start();
        void stop();
        void handle_client(int client_socket);
        bool running() const;
        int get_port() const;
};
