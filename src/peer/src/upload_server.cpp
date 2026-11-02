#include "upload_server.hpp"
#include "logger.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <sstream>

using namespace std;

// Helper đọc đủ n bytes từ socket
static bool recv_exact(int sock, void* buf, size_t n) {
    size_t total = 0;
    char* ptr = static_cast<char*>(buf);
    while (total < n) {
        ssize_t r = recv(sock, ptr + total, n - total, 0);
        if (r <= 0) return false;
        total += r;
    }
    return true;
}

// Helper gửi đủ dữ liệu qua socket
static bool send_all(int sock, const void* buf, size_t n) {
    size_t total = 0;
    const char* ptr = static_cast<const char*>(buf);
    while (total < n) {
        ssize_t s = send(sock, ptr + total, n - total, MSG_NOSIGNAL);
        if (s <= 0) return false;
        total += s;
    }
    return true;
}

static bool send_wire_msg(int sock, const wire_message& msg) {
    vector<uint8_t> data = msg.serialize();
    return send_all(sock, data.data(), data.size());
}

UploadServer::UploadServer(int listen_port, FileManager* fm, StateManager* sm, const string& peer_id)
    : server_fd(-1), port(listen_port), is_running(false), file_manager(fm), state_manager(sm), my_peer_id(peer_id) {}

UploadServer::~UploadServer() {
    stop();
}

void UploadServer::register_file(const string& file_id, const string& file_path, size_t chunk_size) {
    lock_guard<mutex> lock(files_mtx);
    file_paths[file_id] = file_path;
    chunk_sizes[file_id] = chunk_size;
}

bool UploadServer::start() {
    Logger log;
    if (is_running.exchange(true)) {
        return true;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log.err("Failed to create upload server socket");
        is_running = false;
        return false;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log.err("Failed to bind upload server to port " + to_string(port));
        close(server_fd);
        server_fd = -1;
        is_running = false;
        return false;
    }

    if (listen(server_fd, 20) < 0) {
        log.err("Failed to listen on upload server socket");
        close(server_fd);
        server_fd = -1;
        is_running = false;
        return false;
    }

    log.info("UploadServer listening on port " + to_string(port));

    server_thread = thread([this]() {
        while (is_running.load()) {
            struct sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_sock < 0) {
                if (!is_running.load()) break;
                continue;
            }
            thread(&UploadServer::handle_client, this, client_sock).detach();
        }
    });

    return true;
}

void UploadServer::stop() {
    Logger log;
    if (!is_running.exchange(false)) {
        return;
    }

    log.info("Stopping UploadServer on port " + to_string(port) + "...");
    if (server_fd >= 0) {
        shutdown(server_fd, SHUT_RDWR);
        close(server_fd);
        server_fd = -1;
    }

    if (server_thread.joinable()) {
        server_thread.join();
    }
    log.success("UploadServer stopped");
}

void UploadServer::handle_client(int client_sock) {
    Logger log;
    string active_file_id = "";

    // Thiết lập timeout 10s cho kết nối client
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(client_sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    while (is_running.load()) {
        uint32_t net_len = 0;
        if (!recv_exact(client_sock, &net_len, 4)) {
            break;
        }

        uint32_t msg_len = ntohl(net_len);
        if (msg_len == 0) {
            // Keep-alive
            continue;
        }

        vector<uint8_t> body(msg_len);
        if (!recv_exact(client_sock, body.data(), msg_len)) {
            break;
        }

        message_type msg_id = static_cast<message_type>(body[0]);
        vector<uint8_t> payload;
        if (msg_len > 1) {
            payload.assign(body.begin() + 1, body.end());
        }

        if (msg_id == message_type::HANDSHAKE) {
            string payload_str(payload.begin(), payload.end());
            // Payload handshake: chuỗi file_id
            active_file_id = payload_str;
            log.info("Received HANDSHAKE for file " + active_file_id);

            // Gửi lại HANDSHAKE xác nhận
            wire_message hs_res;
            hs_res.id = message_type::HANDSHAKE;
            hs_res.payload.assign(my_peer_id.begin(), my_peer_id.end());
            if (!send_wire_msg(client_sock, hs_res)) break;

            // Gửi BITFIELD của file
            vector<bool> bf;
            if (state_manager) {
                bf = state_manager->get_bitfield(active_file_id);
            }
            if (bf.empty()) {
                lock_guard<mutex> lock(files_mtx);
                if (file_paths.find(active_file_id) != file_paths.end()) {
                    // Nếu là seeder đang giữ file
                    bf = {true};
                }
            }

            wire_message bf_msg;
            bf_msg.id = message_type::BITFIELD;
            for (bool b : bf) {
                bf_msg.payload.push_back(b ? 1 : 0);
            }
            if (!send_wire_msg(client_sock, bf_msg)) break;

        } else if (msg_id == message_type::REQUEST) {
            if (payload.size() < 4) {
                log.err("Invalid REQUEST payload size");
                break;
            }

            uint32_t net_chunk_idx;
            memcpy(&net_chunk_idx, payload.data(), 4);
            uint32_t chunk_idx = ntohl(net_chunk_idx);

            string file_path = "";
            size_t chunk_size = 0;
            {
                lock_guard<mutex> lock(files_mtx);
                if (file_paths.find(active_file_id) != file_paths.end()) {
                    file_path = file_paths[active_file_id];
                    chunk_size = chunk_sizes[active_file_id];
                }
            }

            if (file_path.empty() || !file_manager) {
                log.err("No file registered for file_id " + active_file_id);
                break;
            }

            vector<char> chunk_data = file_manager->read_chunk(file_path, chunk_idx, chunk_size);
            if (chunk_data.empty()) {
                log.err("Failed to read chunk " + to_string(chunk_idx) + " from disk");
                break;
            }

            // Đóng gói PIECE: 4 byte chunk_idx + chunk data
            wire_message piece_msg;
            piece_msg.id = message_type::PIECE;
            piece_msg.payload.resize(4 + chunk_data.size());
            memcpy(piece_msg.payload.data(), &net_chunk_idx, 4);
            memcpy(piece_msg.payload.data() + 4, chunk_data.data(), chunk_data.size());

            if (!send_wire_msg(client_sock, piece_msg)) {
                log.err("Failed to send PIECE message to client");
                break;
            }
            log.info("Sent chunk " + to_string(chunk_idx) + " (" + to_string(chunk_data.size()) + " bytes)");
        }
    }

    close(client_sock);
}

bool UploadServer::running() const {
    return is_running.load();
}

int UploadServer::get_port() const {
    return port;
}
