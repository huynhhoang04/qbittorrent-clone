#include "download_manager.hpp"
#include "logger.hpp"
#include "sha256.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <queue>

using namespace std;

static bool recv_exact_dm(int sock, void* buf, size_t n) {
    size_t total = 0;
    char* ptr = static_cast<char*>(buf);
    while (total < n) {
        ssize_t r = recv(sock, ptr + total, n - total, 0);
        if (r <= 0) return false;
        total += r;
    }
    return true;
}

static bool send_all_dm(int sock, const void* buf, size_t n) {
    size_t total = 0;
    const char* ptr = static_cast<const char*>(buf);
    while (total < n) {
        ssize_t s = send(sock, ptr + total, n - total, MSG_NOSIGNAL);
        if (s <= 0) return false;
        total += s;
    }
    return true;
}

DownloadManager::DownloadManager(FileManager* fm, StateManager* sm, TrackerClient* tc, const string& my_peer_id)
    : file_manager(fm), state_manager(sm), tracker_client(tc), my_peer_id(my_peer_id) {}

void DownloadManager::pause_download(const string& file_id) {
    lock_guard<mutex> lock(mtx);
    paused_files.insert(file_id);
}

void DownloadManager::resume_download(const string& file_id) {
    lock_guard<mutex> lock(mtx);
    paused_files.erase(file_id);
}

bool DownloadManager::is_paused(const string& file_id) {
    lock_guard<mutex> lock(mtx);
    return paused_files.find(file_id) != paused_files.end();
}

bool DownloadManager::download_chunk_from_peer(const peer_info_response& peer, const string& file_id, int chunk_idx, size_t chunk_size, const string& expected_hash, const string& file_path) {
    Logger log;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log.err("Cannot create client socket for peer " + peer.peer_id);
        return false;
    }

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(peer.port);
    if (inet_pton(AF_INET, peer.ip.c_str(), &server_addr.sin_addr) <= 0) {
        log.err("Invalid IP address: " + peer.ip);
        close(sock);
        return false;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log.err("Failed to connect to peer " + peer.peer_id + " at " + peer.ip + ":" + to_string(peer.port));
        close(sock);
        return false;
    }

    // 1. Gửi HANDSHAKE (payload = file_id)
    wire_message hs;
    hs.id = message_type::HANDSHAKE;
    hs.payload.assign(file_id.begin(), file_id.end());
    auto hs_data = hs.serialize();
    if (!send_all_dm(sock, hs_data.data(), hs_data.size())) {
        close(sock);
        return false;
    }

    // Nhận HANDSHAKE phản hồi
    uint32_t net_len = 0;
    if (!recv_exact_dm(sock, &net_len, 4)) {
        close(sock);
        return false;
    }
    uint32_t msg_len = ntohl(net_len);
    vector<uint8_t> hs_res(msg_len);
    if (!recv_exact_dm(sock, hs_res.data(), msg_len)) {
        close(sock);
        return false;
    }

    // Nhận BITFIELD
    if (!recv_exact_dm(sock, &net_len, 4)) {
        close(sock);
        return false;
    }
    msg_len = ntohl(net_len);
    vector<uint8_t> bf_res(msg_len);
    if (!recv_exact_dm(sock, bf_res.data(), msg_len)) {
        close(sock);
        return false;
    }

    // 2. Gửi REQUEST chunk
    wire_message req_msg;
    req_msg.id = message_type::REQUEST;
    req_msg.payload.resize(4);
    uint32_t net_idx = htonl(chunk_idx);
    memcpy(req_msg.payload.data(), &net_idx, 4);

    auto req_data = req_msg.serialize();
    if (!send_all_dm(sock, req_data.data(), req_data.size())) {
        close(sock);
        return false;
    }

    // 3. Nhận PIECE
    if (!recv_exact_dm(sock, &net_len, 4)) {
        close(sock);
        return false;
    }
    msg_len = ntohl(net_len);
    if (msg_len < 5) {
        log.err("Invalid PIECE message length");
        close(sock);
        return false;
    }

    vector<uint8_t> piece_body(msg_len);
    if (!recv_exact_dm(sock, piece_body.data(), msg_len)) {
        close(sock);
        return false;
    }
    close(sock);

    message_type received_id = static_cast<message_type>(piece_body[0]);
    if (received_id != message_type::PIECE) {
        log.err("Unexpected message type: " + to_string((int)received_id));
        return false;
    }

    // Byte 1..4 là chunk_idx, phần còn lại là chunk data
    vector<char> chunk_bytes;
    chunk_bytes.assign(piece_body.begin() + 5, piece_body.end());

    // 4. Xác thực SHA-256
    SHA256 sha;
    string computed_hash = sha.hash_buffer(string(chunk_bytes.data(), chunk_bytes.size()));
    if (!expected_hash.empty() && computed_hash != expected_hash) {
        log.err("Hash mismatch for chunk " + to_string(chunk_idx) + "! Discarding data.");
        return false;
    }

    // 5. Ghi chunk xuống file đĩa
    if (file_manager) {
        if (!file_manager->write_chunk(file_path, chunk_size, chunk_idx, chunk_bytes)) {
            log.err("Failed to write chunk " + to_string(chunk_idx) + " to disk");
            return false;
        }
    }

    // 6. Cập nhật StateManager
    if (state_manager) {
        state_manager->mark_chunk_complete(file_id, chunk_idx);
    }

    log.success("Chunk " + to_string(chunk_idx) + " downloaded and verified successfully");
    return true;
}

bool DownloadManager::start_download(const torrent_info& meta, const string& file_path, int num_threads) {
    Logger log;
    string file_id = meta.file_hash;
    log.info("Starting download for file " + meta.file_name + " (" + file_id + ")");

    // Tạo file rỗng nếu chưa có
    if (file_manager && !filesystem::exists(file_path)) {
        file_manager->create_empty_file(file_path, meta.file_size);
    }

    if (state_manager) {
        state_manager->init_file_state(file_id, meta.num_chunks, false);
    }

    // Truy vấn Tracker lấy danh sách peer
    vector<peer_info_response> all_peers;
    if (tracker_client) {
        all_peers = tracker_client->get_peers(file_id);
    }

    vector<peer_info_response> peers;
    for (const auto& p : all_peers) {
        if (p.peer_id != my_peer_id) {
            peers.push_back(p);
        }
    }

    if (peers.empty()) {
        log.warnning("No remote peers available for file " + file_id + ". Waiting for peers...");
        return false;
    }

    auto missing = state_manager ? state_manager->get_missing_chunks(file_id) : vector<int>();
    if (missing.empty()) {
        log.success("File " + meta.file_name + " already downloaded completely!");
        return true;
    }

    log.info("Missing " + to_string(missing.size()) + " / " + to_string(meta.num_chunks) + " chunks. Launching download workers...");

    queue<int> chunk_queue;
    for (int idx : missing) {
        chunk_queue.push(idx);
    }
    mutex queue_mtx;

    atomic<int> completed_count{0};
    int total_to_download = missing.size();
    vector<thread> workers;

    int actual_threads = min((int)num_threads, total_to_download);
    if (actual_threads < 1) actual_threads = 1;

    for (int t = 0; t < actual_threads; ++t) {
        workers.emplace_back([this, &chunk_queue, &queue_mtx, &peers, &meta, &file_path, &completed_count]() {
            while (true) {
                if (is_paused(meta.file_hash)) {
                    this_thread::sleep_for(chrono::milliseconds(500));
                    continue;
                }

                int chunk_idx = -1;
                {
                    lock_guard<mutex> lock(queue_mtx);
                    if (chunk_queue.empty()) {
                        break;
                    }
                    chunk_idx = chunk_queue.front();
                    chunk_queue.pop();
                }

                string expected_hash = "";
                if ((size_t)chunk_idx < meta.chunk_hashs.size()) {
                    expected_hash = meta.chunk_hashs[chunk_idx];
                }

                bool downloaded = false;
                for (const auto& peer : peers) {
                    if (download_chunk_from_peer(peer, meta.file_hash, chunk_idx, meta.chunk_size, expected_hash, file_path)) {
                        downloaded = true;
                        completed_count++;
                        break;
                    }
                }

                // Nếu không tải được từ bất kỳ peer nào, đưa lại vào queue để retry sau
                if (!downloaded) {
                    lock_guard<mutex> lock(queue_mtx);
                    chunk_queue.push(chunk_idx);
                    this_thread::sleep_for(chrono::milliseconds(200));
                }
            }
        });
    }

    for (auto& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }

    if (state_manager && state_manager->is_completed(file_id)) {
        log.success("Download completed for " + meta.file_name + " (100%)");
        return true;
    }

    return false;
}
