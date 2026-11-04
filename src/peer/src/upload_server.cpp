#include "upload_server.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>

#include "logger.hpp"

using namespace std;

UploadServer::UploadServer(int listen_port, FileManager* fm, StateManager* sm,
                           const string& peer_id)
    : port(listen_port),
      file_manager(fm),
      state_manager(sm),
      my_peer_id(peer_id) {}

UploadServer::~UploadServer() {
    stop();
}

void UploadServer::register_file(const string& file_id, const string& file_path,
                                 size_t chunk_size, size_t num_chunks) {
    if (file_id.empty() || file_path.empty() || chunk_size == 0) {
        return;
    }

    size_t chunks = num_chunks;
    size_t file_size = 0;
    error_code ec;
    const auto size = filesystem::file_size(file_path, ec);
    if (!ec) {
        file_size = static_cast<size_t>(size);
    }
    if (chunks == 0 && !ec && chunk_size > 0) {
        chunks = (file_size + chunk_size - 1) / chunk_size;
    }

    lock_guard<mutex> lock(files_mtx);
    files[file_id] = file_entry{file_path, chunk_size, chunks, file_size};
}

void UploadServer::set_upload_options(const upload_options& new_options) {
    lock_guard<mutex> lock(slot_mtx);
    options = new_options;
    slot_cv.notify_all();
}

upload_options UploadServer::get_upload_options() const {
    lock_guard<mutex> lock(slot_mtx);
    return options;
}

void UploadServer::set_max_active_uploads(int limit) {
    lock_guard<mutex> lock(slot_mtx);
    options.max_active_uploads = std::max(1, std::min(limit, 128));
    slot_cv.notify_all();
}

int UploadServer::get_max_active_uploads() const {
    lock_guard<mutex> lock(slot_mtx);
    return options.max_active_uploads;
}

bool UploadServer::start() {
    Logger log;
    if (is_running.exchange(true)) {
        return true;
    }

    listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (!net_valid(listen_socket)) {
        log.err("Failed to create upload server socket");
        is_running.store(false);
        return false;
    }

    int reuse = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    struct sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(listen_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        log.err("Failed to bind upload server to port " + to_string(port) + ": " +
                net_last_error());
        net_close(listen_socket);
        listen_socket = invalid_socket;
        is_running.store(false);
        return false;
    }

    if (port == 0) {
        struct sockaddr_in bound {};
        socklen_t bound_length = sizeof(bound);
        if (getsockname(listen_socket, reinterpret_cast<sockaddr*>(&bound),
                        &bound_length) == 0) {
            port = ntohs(bound.sin_port);
        }
    }

    // A listening socket is not connected, so shutdown() cannot wake a blocked
    // accept() on Winsock. Poll with a receive timeout instead, otherwise
    // stop() deadlocks in accept_thread.join() whenever no new peer arrives.
    net_set_timeouts(listen_socket, 500);

    if (listen(listen_socket, 32) < 0) {
        log.err("Failed to listen on upload server socket");
        net_close(listen_socket);
        listen_socket = invalid_socket;
        is_running.store(false);
        return false;
    }

    log.info("UploadServer listening on port " + to_string(port));

    accept_thread = thread(&UploadServer::accept_loop, this);

    int worker_count = options.worker_threads;
    if (worker_count <= 0) {
        worker_count = std::max(4, options.max_active_uploads + 3);
    }
    for (int i = 0; i < worker_count; ++i) {
        workers.emplace_back(&UploadServer::worker_loop, this);
    }

    return true;
}

void UploadServer::stop() {
    Logger log;
    if (!is_running.exchange(false)) {
        return;
    }

    log.info("Stopping UploadServer on port " + to_string(port) + "...");

    if (net_valid(listen_socket)) {
        net_shutdown(listen_socket);
        net_close(listen_socket);
        listen_socket = invalid_socket;
    }

    queue_cv.notify_all();
    slot_cv.notify_all();

    if (accept_thread.joinable()) {
        accept_thread.join();
    }

    {
        lock_guard<mutex> lock(queue_mtx);
        for (socket_t socket : pending_sockets) {
            net_shutdown(socket);
            net_close(socket);
        }
        pending_sockets.clear();
    }

    {
        lock_guard<mutex> lock(sockets_mtx);
        for (socket_t socket : active_sockets) {
            net_shutdown(socket);
        }
    }

    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers.clear();

    log.success("UploadServer stopped");
}

void UploadServer::accept_loop() {
    Logger log;
    while (is_running.load()) {
        struct sockaddr_in client_address {};
        socklen_t client_length = sizeof(client_address);
        const socket_t client =
            accept(listen_socket, reinterpret_cast<sockaddr*>(&client_address),
                   &client_length);
        if (!net_valid(client)) {
            if (!is_running.load()) {
                break;
            }
            continue;
        }

        if (!net_set_timeouts(client, options.io_timeout_ms)) {
            net_close(client);
            continue;
        }

        bool queued = false;
        {
            lock_guard<mutex> lock(queue_mtx);
            if (pending_sockets.size() < options.queue_capacity && is_running.load()) {
                pending_sockets.push_back(client);
                queued = true;
            }
        }

        if (!queued) {
            log.warnning("Upload queue is full; refusing an incoming connection");
            net_close(client);
            continue;
        }
        queue_cv.notify_one();
    }
}

void UploadServer::worker_loop() {
    while (true) {
        socket_t client = invalid_socket;
        {
            unique_lock<mutex> lock(queue_mtx);
            queue_cv.wait(lock, [this] {
                return !pending_sockets.empty() || !is_running.load();
            });
            if (pending_sockets.empty()) {
                if (!is_running.load()) {
                    return;
                }
                continue;
            }
            client = pending_sockets.front();
            pending_sockets.pop_front();
        }

        if (!net_valid(client)) {
            continue;
        }
        if (!is_running.load()) {
            net_close(client);
            continue;
        }

        handle_client(client);
    }
}

bool UploadServer::acquire_upload_slot(int timeout_ms) {
    unique_lock<mutex> lock(slot_mtx);
    const auto predicate = [this] {
        return active_slots < options.max_active_uploads || !is_running.load();
    };

    if (!predicate()) {
        slot_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), predicate);
    }
    if (!is_running.load() || active_slots >= options.max_active_uploads) {
        return false;
    }

    ++active_slots;
    peak_slots = std::max(peak_slots, active_slots);
    return true;
}

void UploadServer::release_upload_slot() {
    {
        lock_guard<mutex> lock(slot_mtx);
        if (active_slots > 0) {
            --active_slots;
        }
    }
    slot_cv.notify_all();
}

void UploadServer::track_socket(socket_t socket, bool add) {
    lock_guard<mutex> lock(sockets_mtx);
    if (add) {
        active_sockets.insert(socket);
    } else {
        active_sockets.erase(socket);
    }
}

void UploadServer::handle_client(socket_t client_socket) {
    Logger log;
    string peer_id = "unknown";
    string file_id;
    string client_ip;
    bool holds_slot = false;
    bool interested = false;

    track_socket(client_socket, true);

    struct sockaddr_in peer_address {};
    socklen_t peer_length = sizeof(peer_address);
    if (getpeername(client_socket, reinterpret_cast<sockaddr*>(&peer_address),
                    &peer_length) == 0) {
        char buffer[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &peer_address.sin_addr, buffer, sizeof(buffer));
        client_ip = buffer;
    }

    auto send_message = [&](message_type type) {
        wire_message message;
        message.id = type;
        return send_wire_message(client_socket, message);
    };

    auto publish_status = [&](bool choked) {
        lock_guard<mutex> lock(sessions_mtx);
        auto found = session_status.find(peer_id);
        if (found == session_status.end()) {
            upload_peer_status status;
            status.peer_id = peer_id;
            status.ip = client_ip;
            status.port = peer_address.sin_port == 0 ? 0 : ntohs(peer_address.sin_port);
            status.choked = choked;
            session_status.emplace(peer_id, status);
        } else {
            found->second.choked = choked;
        }
    };

    do {
        string error;
        wire_message handshake;
        if (!recv_wire_message(client_socket, handshake, error)) {
            break;
        }
        if (handshake.id != message_type::HANDSHAKE) {
            log.warnning("Rejected connection: expected HANDSHAKE, received " +
                         string(message_type_name(handshake.id)));
            break;
        }

        handshake_info info;
        if (!decode_handshake(handshake.payload, info)) {
            log.warnning("Rejected connection with malformed handshake");
            break;
        }

        peer_id = info.peer_id;
        file_id = info.file_id;

        file_entry entry;
        {
            lock_guard<mutex> lock(files_mtx);
            const auto found = files.find(file_id);
            if (found == files.end()) {
                log.warnning("Peer " + peer_id + " requested an unknown file id");
                break;
            }
            entry = found->second;
        }

        wire_message response;
        response.id = message_type::HANDSHAKE;
        response.payload = encode_handshake({my_peer_id, file_id});
        if (!send_wire_message(client_socket, response)) {
            break;
        }

        vector<bool> bitfield;
        if (state_manager) {
            bitfield = state_manager->get_bitfield(file_id);
        }

        wire_message bitfield_message;
        bitfield_message.id = message_type::BITFIELD;
        bitfield_message.payload.reserve(bitfield.size());
        for (bool bit : bitfield) {
            bitfield_message.payload.push_back(bit ? 1 : 0);
        }
        if (!send_wire_message(client_socket, bitfield_message)) {
            break;
        }

        publish_status(true);

        while (is_running.load()) {
            wire_message message;
            if (!recv_wire_message(client_socket, message, error)) {
                break;
            }

            if (message.id == message_type::KEEPALIVE) {
                continue;
            }

            if (message.id == message_type::INTERESTED) {
                if (holds_slot) {
                    continue;
                }
                interested = true;
                if (acquire_upload_slot(options.choke_wait_ms)) {
                    holds_slot = true;
                    publish_status(false);
                    if (!send_message(message_type::UNCHOKE)) {
                        break;
                    }
                } else {
                    publish_status(true);
                    send_message(message_type::CHOKE);
                    break;
                }
                continue;
            }

            if (message.id == message_type::NOT_INTERESTED) {
                interested = false;
                send_message(message_type::CHOKE);
                if (holds_slot) {
                    release_upload_slot();
                    holds_slot = false;
                }
                publish_status(true);
                continue;
            }

            if (message.id == message_type::CANCEL) {
                continue;
            }

            if (message.id != message_type::REQUEST) {
                log.warnning("Closing connection: unexpected " +
                             string(message_type_name(message.id)) + " from " + peer_id);
                break;
            }

            if (!interested || !holds_slot) {
                // A choked client is never served, even if it ignores CHOKE.
                // The slot state is deliberately left untouched here: this
                // connection does not own an upload slot yet.
                send_message(message_type::CHOKE);
                continue;
            }

            if (message.payload.size() < 4) {
                log.warnning("Closing connection: malformed REQUEST from " + peer_id);
                break;
            }

            uint32_t net_index = 0;
            memcpy(&net_index, message.payload.data(), sizeof(net_index));
            const uint32_t chunk_index = ntohl(net_index);

            if (chunk_index >= entry.num_chunks) {
                // Requesting an out-of-range chunk is a protocol violation.
                // Closing releases the upload slot in the cleanup path below
                // instead of leaving the client choked while the slot leaks.
                log.warnning("Closing connection: peer " + peer_id +
                             " requested out-of-range chunk " +
                             to_string(chunk_index));
                break;
            }

            if (state_manager &&
                !state_manager->has_chunk(file_id, static_cast<int>(chunk_index))) {
                log.warnning("Closing connection: peer " + peer_id +
                             " requested chunk " + to_string(chunk_index) +
                             " that this peer does not hold");
                break;
            }

            if (!file_manager) {
                log.warnning("Closing connection: no file manager configured");
                break;
            }

            vector<char> data = file_manager->read_chunk(entry.path, chunk_index,
                                                         entry.chunk_size);
            const size_t offset = static_cast<size_t>(chunk_index) * entry.chunk_size;
            size_t expected_size = 0;
            if (entry.file_size > 0 && offset < entry.file_size) {
                expected_size =
                    std::min(entry.chunk_size, entry.file_size - offset);
            }
            if (data.empty() || (expected_size > 0 && data.size() != expected_size) ||
                (expected_size == 0 && data.size() > entry.chunk_size)) {
                log.warnning("Closing connection: cannot serve chunk " +
                             to_string(chunk_index) + " for file " + file_id);
                break;
            }

            {
                lock_guard<mutex> lock(files_mtx);
                if (corrupt_chunks.count(static_cast<int>(chunk_index)) != 0 &&
                    !data.empty()) {
                    data[0] = static_cast<char>(data[0] ^ 0x5A);
                }
            }

            wire_message piece;
            piece.id = message_type::PIECE;
            piece.payload.resize(4 + data.size());
            memcpy(piece.payload.data(), &net_index, sizeof(net_index));
            memcpy(piece.payload.data() + 4, data.data(), data.size());

            const int delay_ms = options.chunk_delay_ms;
            if (delay_ms > 0) {
                this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            }

            if (!send_wire_message(client_socket, piece)) {
                break;
            }

            bytes_uploaded.fetch_add(static_cast<uint64_t>(data.size()));
            chunks_uploaded.fetch_add(1);
            {
                lock_guard<mutex> lock(sessions_mtx);
                auto& status = session_status[peer_id];
                status.chunks_uploaded++;
                status.bytes_uploaded += data.size();
            }
        }
    } while (false);

    if (holds_slot) {
        release_upload_slot();
    }
    {
        lock_guard<mutex> lock(sessions_mtx);
        session_status.erase(peer_id);
    }
    track_socket(client_socket, false);
    net_shutdown(client_socket);
    net_close(client_socket);
}

bool UploadServer::running() const {
    return is_running.load();
}

int UploadServer::get_port() const {
    return port;
}

vector<upload_peer_status> UploadServer::get_peer_status() const {
    lock_guard<mutex> lock(sessions_mtx);
    vector<upload_peer_status> result;
    result.reserve(session_status.size());
    for (const auto& entry : session_status) {
        result.push_back(entry.second);
    }
    return result;
}

uint64_t UploadServer::total_bytes_uploaded() const {
    return bytes_uploaded.load();
}

uint64_t UploadServer::total_chunks_uploaded() const {
    return chunks_uploaded.load();
}

int UploadServer::active_upload_slots() const {
    lock_guard<mutex> lock(slot_mtx);
    return active_slots;
}

int UploadServer::peak_upload_slots() const {
    lock_guard<mutex> lock(slot_mtx);
    return peak_slots;
}

void UploadServer::set_corrupt_chunks(const vector<int>& chunks) {
    lock_guard<mutex> lock(files_mtx);
    corrupt_chunks.clear();
    for (int chunk : chunks) {
        if (chunk >= 0) {
            corrupt_chunks.insert(chunk);
        }
    }
}

void UploadServer::clear_corrupt_chunks() {
    lock_guard<mutex> lock(files_mtx);
    corrupt_chunks.clear();
}
