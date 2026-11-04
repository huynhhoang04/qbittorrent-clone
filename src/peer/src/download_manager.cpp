#include "download_manager.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <random>

#include "logger.hpp"
#include "sha256.hpp"

using namespace std;
using namespace std::chrono;

namespace {

uint32_t encode_chunk_index(int chunk_index) {
    const uint32_t value = static_cast<uint32_t>(chunk_index);
    return htonl(value);
}

bool decode_chunk_index(const vector<uint8_t>& payload, int& chunk_index) {
    if (payload.size() < 4) {
        return false;
    }
    uint32_t net_value = 0;
    memcpy(&net_value, payload.data(), 4);
    chunk_index = static_cast<int>(ntohl(net_value));
    return true;
}

mt19937& random_engine() {
    static thread_local mt19937 engine(std::random_device{}());
    return engine;
}

} // namespace

DownloadManager::DownloadManager(FileManager* fm, StateManager* sm, TrackerClient* tc,
                                 const string& my_peer_id)
    : file_manager(fm),
      state_manager(sm),
      tracker_client(tc),
      my_peer_id(my_peer_id),
      max_active_downloads(download_options{}.max_active_downloads) {}

DownloadManager::~DownloadManager() {
    // The owner must join the thread that called start_download before this
    // destructor runs; request_stop only unblocks a running transfer.
    request_stop();
}

void DownloadManager::set_options(const download_options& new_options) {
    lock_guard<mutex> lock(mtx);
    options = new_options;
    max_active_downloads.store(options.max_active_downloads);
}

download_options DownloadManager::get_options() const {
    lock_guard<mutex> lock(mtx);
    return options;
}

void DownloadManager::set_max_active_downloads(int limit) {
    max_active_downloads.store(std::max(1, std::min(limit, 64)));
}

int DownloadManager::get_max_active_downloads() const {
    return max_active_downloads.load();
}

void DownloadManager::set_chunk_verified_callback(chunk_verified_callback callback) {
    lock_guard<mutex> lock(mtx);
    verified_callback = std::move(callback);
}

void DownloadManager::pause_download(const string& file_id) {
    if (file_id.empty() || file_id == current_file_id) {
        paused_flag.store(true);
        cv.notify_all();
    }
}

void DownloadManager::resume_download(const string& file_id) {
    if (file_id.empty() || file_id == current_file_id) {
        paused_flag.store(false);
        cv.notify_all();
    }
}

bool DownloadManager::is_paused(const string& file_id) {
    if (file_id.empty() || file_id == current_file_id) {
        return paused_flag.load();
    }
    return false;
}

void DownloadManager::request_stop() {
    stop_flag.store(true);
    paused_flag.store(false);
    cv.notify_all();

    vector<shared_ptr<peer_session>> current;
    {
        lock_guard<mutex> lock(mtx);
        for (auto& entry : sessions) {
            current.push_back(entry.second);
        }
    }
    for (auto& session : current) {
        const socket_t sock = session->sock.load();
        if (net_valid(sock)) {
            net_shutdown(sock);
        }
    }
}

bool DownloadManager::is_active() const {
    return active_flag.load();
}

string DownloadManager::last_error() const {
    lock_guard<mutex> lock(mtx);
    return last_error_text;
}

size_t DownloadManager::chunk_size_for(int chunk_index) const {
    if (chunk_index < 0 || static_cast<size_t>(chunk_index) >= current_meta.num_chunks) {
        return 0;
    }
    const size_t offset = static_cast<size_t>(chunk_index) * current_meta.chunk_size;
    if (offset >= current_meta.file_size) {
        return 0;
    }
    return std::min(current_meta.chunk_size, current_meta.file_size - offset);
}

string DownloadManager::expected_hash(int chunk_index) const {
    if (chunk_index < 0) {
        return "";
    }
    const size_t index = static_cast<size_t>(chunk_index);
    if (index < current_meta.chunk_hashs.size()) {
        return current_meta.chunk_hashs[index];
    }
    return "";
}

bool DownloadManager::work_remaining() {
    lock_guard<mutex> lock(mtx);
    return !pending_chunks.empty() || !claimed_chunks.empty();
}

void DownloadManager::wait_for_work(int milliseconds) {
    unique_lock<mutex> lock(mtx);
    cv.wait_for(lock, std::chrono::milliseconds(milliseconds));
}

void DownloadManager::record_failure(const string& peer_id, const string& message) {
    {
        lock_guard<mutex> lock(mtx);
        last_error_text = message;
    }
    Logger log;
    log.err((peer_id.empty() ? string("download") : peer_id) + ": " + message);
}

void DownloadManager::complete_chunk(int chunk_index) {
    {
        lock_guard<mutex> lock(mtx);
        claimed_chunks.erase(chunk_index);
    }
    cv.notify_all();
}

bool DownloadManager::verify_existing_chunks() {
    if (!state_manager || !file_manager) {
        return false;
    }

    Logger log;
    const vector<int> verified = state_manager->get_verified_chunks(current_file_id);
    size_t invalidated = 0;
    for (int index : verified) {
        const size_t expected_size = chunk_size_for(index);
        if (expected_size == 0) {
            state_manager->mark_chunk_missing(current_file_id, index);
            invalidated++;
            continue;
        }
        const vector<char> data =
            file_manager->read_chunk(current_file_path, index, current_meta.chunk_size);
        const string expected = expected_hash(index);
        if (data.size() != expected_size) {
            state_manager->mark_chunk_missing(current_file_id, index);
            invalidated++;
            continue;
        }
        if (!expected.empty()) {
            SHA256 sha;
            const string actual = sha.hash_buffer(string(data.data(), data.size()));
            if (actual != expected) {
                state_manager->mark_chunk_missing(current_file_id, index);
                invalidated++;
            }
        }
    }

    if (invalidated > 0) {
        log.warnning("Invalidated " + to_string(invalidated) +
                     " persisted chunk(s) whose content no longer matches metadata");
    }
    return true;
}

bool DownloadManager::verify_whole_file() {
    if (!file_manager) {
        record_failure("", "file manager is not configured");
        return false;
    }
    if (current_meta.file_hash.empty()) {
        record_failure("", "torrent metadata has no whole-file hash");
        return false;
    }

    SHA256 sha;
    const string actual = sha.hash_file(current_file_path);
    if (actual != current_meta.file_hash) {
        record_failure("", "whole-file SHA-256 mismatch after download");
        return false;
    }
    return true;
}

bool DownloadManager::discover_peers() {
    if (!tracker_client) {
        return false;
    }

    vector<peer_info_response> discovered = tracker_client->get_peers(current_file_id);
    if (discovered.empty()) {
        return false;
    }

    lock_guard<mutex> lock(mtx);
    bool added = false;
    for (const auto& peer : discovered) {
        if (peer.peer_id.empty() || peer.peer_id == my_peer_id) {
            continue;
        }
        if (peer.port <= 0 || peer.port > 65535 || peer.ip.empty()) {
            continue;
        }
        auto existing = sessions.find(peer.peer_id);
        if (existing != sessions.end()) {
            // An established session only ever reads the bitfield it captured at
            // connect time, so a peer that was empty then and filled up later
            // would stay invisible: rarest-first would keep picking the old
            // sources and a failover could never see the new one. Refresh it from
            // the tracker's answer and wake the workers waiting on availability.
            if (peer.bitfield.size() == current_meta.num_chunks &&
                existing->second->bitfield != peer.bitfield) {
                existing->second->bitfield = peer.bitfield;
                added = true;
            }
            continue;
        }

        const auto known =
            std::find_if(known_peers.begin(), known_peers.end(),
                         [&](const peer_info_response& candidate) {
                             return candidate.peer_id == peer.peer_id;
                         });
        if (known != known_peers.end()) {
            if (peer.bitfield.size() == current_meta.num_chunks &&
                known->bitfield != peer.bitfield) {
                known->bitfield = peer.bitfield;
                known->ip = peer.ip;
                known->port = peer.port;
                added = true;
            }
            continue;
        }

        known_peers.push_back(peer);
        added = true;
    }
    return added;
}

void DownloadManager::start_available_sessions() {
    const int limit = max_active_downloads.load();
    vector<shared_ptr<peer_session>> started;
    vector<thread> retired_workers;

    {
        lock_guard<mutex> lock(mtx);
        if (stop_flag.load()) {
            return;
        }

        for (const auto& peer : known_peers) {
            if (active_peers.load() >= limit) {
                break;
            }
            if (peer.peer_id == my_peer_id) {
                continue;
            }

            auto existing = sessions.find(peer.peer_id);
            if (existing != sessions.end()) {
                const auto& session = existing->second;
                if (!session->finished.load()) {
                    continue;
                }

                // Restarting a finished session is only useful when it still
                // owns at least one of the chunks that are pending right now.
                // This avoids a reconnect loop against a peer that cannot help.
                bool useful = false;
                for (int index : pending_chunks) {
                    if (static_cast<size_t>(index) < session->bitfield.size() &&
                        session->bitfield[index]) {
                        const auto banned = blacklisted_peers.find(index);
                        if (banned == blacklisted_peers.end() ||
                            banned->second.count(peer.peer_id) == 0) {
                            useful = true;
                            break;
                        }
                    }
                }
                if (!useful) {
                    continue;
                }
                if (peer_failures[peer.peer_id] >= options.max_peer_failures) {
                    continue;
                }
                if (session->worker.joinable()) {
                    retired_workers.push_back(std::move(session->worker));
                }
                sessions.erase(existing);
            } else if (peer_failures[peer.peer_id] >= options.max_peer_failures) {
                continue;
            }

            auto session = make_shared<peer_session>();
            session->peer = peer;
            sessions[peer.peer_id] = session;
            tried_peers.insert(peer.peer_id);

            const int current = active_peers.fetch_add(1) + 1;
            int previous_peak = peak_active_peers.load();
            while (current > previous_peak &&
                   !peak_active_peers.compare_exchange_weak(previous_peak, current)) {
            }

            started.push_back(session);
        }
    }

    for (auto& worker : retired_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    for (auto& session : started) {
        try {
            session->worker = thread(&DownloadManager::peer_worker, this, session);
        } catch (const exception& e) {
            session->finished.store(true);
            active_peers.fetch_sub(1);
            record_failure(session->peer.peer_id,
                           "failed to start download worker: " + string(e.what()));
        }
    }
}

int DownloadManager::claim_chunk(const shared_ptr<peer_session>& session) {
    lock_guard<mutex> lock(mtx);

    for (auto it = pending_chunks.begin(); it != pending_chunks.end();) {
        const int index = *it;
        const auto attempts = chunk_attempts.find(index);
        if (attempts != chunk_attempts.end() &&
            attempts->second >= options.max_attempts_per_chunk) {
            failed_chunks.insert(index);
            it = pending_chunks.erase(it);
        } else {
            ++it;
        }
    }

    if (pending_chunks.empty()) {
        return -1;
    }

    vector<int> candidates;
    candidates.reserve(pending_chunks.size());
    for (int index : pending_chunks) {
        if (index < 0 || static_cast<size_t>(index) >= session->bitfield.size() ||
            !session->bitfield[index]) {
            continue;
        }
        const auto banned = blacklisted_peers.find(index);
        if (banned != blacklisted_peers.end() &&
            banned->second.count(session->peer.peer_id) != 0) {
            continue;
        }
        candidates.push_back(index);
    }

    if (candidates.empty()) {
        return -1;
    }

    int chosen = -1;
    if (random_first_pending) {
        // The first chunk is selected uniformly at random so the peer obtains
        // some verified data (and becomes useful to the swarm) as soon as
        // possible.
        uniform_int_distribution<size_t> distribution(0, candidates.size() - 1);
        chosen = candidates[distribution(random_engine())];
        random_first_pending = false;
    } else {
        unordered_map<int, int> availability;
        for (int index : candidates) {
            int count = 0;
            for (const auto& entry : sessions) {
                const vector<bool>& bitfield = entry.second->bitfield;
                if (static_cast<size_t>(index) < bitfield.size() && bitfield[index]) {
                    count++;
                }
            }
            availability[index] = count;
        }

        int rarest = numeric_limits<int>::max();
        for (const auto& entry : availability) {
            rarest = std::min(rarest, entry.second);
        }

        vector<int> rarest_candidates;
        for (int index : candidates) {
            if (availability[index] == rarest) {
                rarest_candidates.push_back(index);
            }
        }

        uniform_int_distribution<size_t> distribution(0, rarest_candidates.size() - 1);
        chosen = rarest_candidates[distribution(random_engine())];
    }

    if (claimed_chunks.count(chosen) != 0) {
        duplicate_claims.fetch_add(1);
        return -1;
    }

    pending_chunks.erase(chosen);
    claimed_chunks.insert(chosen);
    chunk_attempts[chosen] = chunk_attempts[chosen] + 1;
    return chosen;
}

void DownloadManager::release_chunk(int chunk_index, const string& peer_id, bool retryable) {
    bool failed = false;
    {
        lock_guard<mutex> lock(mtx);
        claimed_chunks.erase(chunk_index);

        const int attempts = chunk_attempts[chunk_index];
        if (retryable && attempts < options.max_attempts_per_chunk) {
            pending_chunks.insert(chunk_index);
            retry_count.fetch_add(1);
        } else {
            pending_chunks.erase(chunk_index);
            failed_chunks.insert(chunk_index);
            failed = true;
        }
    }

    if (failed) {
        record_failure("", "chunk " + to_string(chunk_index) + " exhausted its retry budget");
    }
    cv.notify_all();
}

void DownloadManager::release_all(vector<int>& in_flight, const string& peer_id,
                                  bool retryable) {
    for (int index : in_flight) {
        release_chunk(index, peer_id, retryable);
    }
    in_flight.clear();
}

void DownloadManager::peer_worker(shared_ptr<peer_session> session) {
    Logger log;
    const string peer_id = session->peer.peer_id;
    vector<int> in_flight;
    int unclaimable_rounds = 0;
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    string failure;

    auto finish = [&]() {
        const socket_t active = session->sock.exchange(invalid_socket);
        if (net_valid(active)) {
            net_shutdown(active);
            net_close(active);
        } else if (net_valid(sock)) {
            net_shutdown(sock);
            net_close(sock);
        }

        release_all(in_flight, peer_id, true);

        {
            lock_guard<mutex> lock(mtx);
            session->last_error = failure;
            if (!failure.empty()) {
                peer_failures[peer_id] = peer_failures[peer_id] + 1;
                if (last_error_text.empty()) {
                    last_error_text = failure;
                }
            } else {
                // A session that ended cleanly proved the peer is healthy, so
                // its earlier transient timeouts must not keep counting. Without
                // this reset, three slow rounds permanently drop a working seeder
                // from a swarm that has no other source for the last chunks.
                peer_failures[peer_id] = 0;
            }
        }

        session->connected.store(false);
        session->choked.store(true);
        session->finished.store(true);
        active_peers.fetch_sub(1);
        cv.notify_all();

        if (!failure.empty()) {
            log.warnning("Peer " + peer_id + " session ended: " + failure);
        }
    };

    if (!file_manager || !state_manager) {
        failure = "download manager is not fully configured";
        finish();
        return;
    }
    if (!net_valid(sock)) {
        failure = "cannot create socket: " + net_last_error();
        finish();
        return;
    }

    do {
        if (stop_flag.load()) {
            break;
        }

        net_set_timeouts(sock, options.io_timeout_ms);

        struct sockaddr_in address {};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<uint16_t>(session->peer.port));
        if (inet_pton(AF_INET, session->peer.ip.c_str(), &address.sin_addr) != 1) {
            failure = "invalid peer IP address";
            break;
        }

        if (!net_connect_timeout(sock, reinterpret_cast<sockaddr*>(&address),
                                 sizeof(address), options.connect_timeout_ms)) {
            failure = "connect failed";
            break;
        }

        session->sock.store(sock);
        session->connected.store(true);

        wire_message handshake;
        handshake.id = message_type::HANDSHAKE;
        handshake.payload = encode_handshake({my_peer_id, current_file_id});
        if (!send_wire_message(sock, handshake)) {
            failure = "failed to send handshake";
            break;
        }

        wire_message response;
        string receive_error;
        if (!recv_wire_message(sock, response, receive_error)) {
            failure = "handshake response failed: " + receive_error;
            break;
        }
        if (response.id != message_type::HANDSHAKE) {
            failure = string("expected HANDSHAKE, received ") +
                      message_type_name(response.id);
            break;
        }

        handshake_info remote_info;
        if (decode_handshake(response.payload, remote_info)) {
            if (!remote_info.file_id.empty() && remote_info.file_id != current_file_id) {
                failure = "peer answered for a different file";
                break;
            }
        }

        wire_message bitfield_message;
        if (!recv_wire_message(sock, bitfield_message, receive_error)) {
            failure = "bitfield receive failed: " + receive_error;
            break;
        }
        if (bitfield_message.id != message_type::BITFIELD) {
            failure = string("expected BITFIELD, received ") +
                      message_type_name(bitfield_message.id);
            break;
        }
        if (bitfield_message.payload.size() != current_meta.num_chunks) {
            failure = "bitfield size does not match torrent metadata";
            break;
        }

        vector<bool> received_bitfield(bitfield_message.payload.size(), false);
        bool has_any = false;
        for (size_t i = 0; i < bitfield_message.payload.size(); ++i) {
            received_bitfield[i] = bitfield_message.payload[i] != 0;
            has_any = has_any || received_bitfield[i];
        }
        if (!has_any) {
            failure = "peer advertises no chunks";
            break;
        }
        {
            // claim_chunk() reads every session bitfield while holding mtx, so
            // publish the newly received bitfield under the same lock.
            lock_guard<mutex> lock(mtx);
            session->bitfield = std::move(received_bitfield);
        }

        wire_message interested;
        interested.id = message_type::INTERESTED;
        if (!send_wire_message(sock, interested)) {
            failure = "failed to send INTERESTED";
            break;
        }

        // A client starts choked. Requests are only sent after UNCHOKE.
        bool unchoked = false;
        const auto unchoke_deadline =
            steady_clock::now() + milliseconds(std::max(options.io_timeout_ms, 10000));
        while (!stop_flag.load() && steady_clock::now() < unchoke_deadline) {
            wire_message message;
            if (!recv_wire_message(sock, message, receive_error)) {
                failure = "timeout waiting for UNCHOKE: " + receive_error;
                break;
            }
            if (message.id == message_type::UNCHOKE) {
                unchoked = true;
                break;
            }
            if (message.id == message_type::CHOKE ||
                message.id == message_type::KEEPALIVE ||
                message.id == message_type::BITFIELD) {
                continue;
            }
            failure = string("unexpected message while choked: ") +
                      message_type_name(message.id);
            break;
        }

        if (!failure.empty()) {
            break;
        }
        if (!unchoked) {
            failure = "peer did not grant an upload slot";
            break;
        }
        session->choked.store(false);

        while (!stop_flag.load()) {
            if (!work_remaining()) {
                break;
            }

            if (paused_flag.load()) {
                wait_for_work(100);
                continue;
            }

            if (session->choked.load()) {
                const auto deadline =
                    steady_clock::now() + milliseconds(std::max(options.io_timeout_ms, 10000));
                bool granted = false;
                while (!stop_flag.load() && steady_clock::now() < deadline) {
                    wire_message message;
                    if (!recv_wire_message(sock, message, receive_error)) {
                        failure = "receive failed while choked: " + receive_error;
                        break;
                    }
                    if (message.id == message_type::UNCHOKE) {
                        granted = true;
                        break;
                    }
                    if (message.id == message_type::CHOKE ||
                        message.id == message_type::KEEPALIVE) {
                        continue;
                    }
                    failure = string("unexpected message while choked: ") +
                              message_type_name(message.id);
                    break;
                }
                if (!failure.empty()) {
                    break;
                }
                if (!granted) {
                    failure = "peer did not re-unchoke";
                    break;
                }
                session->choked.store(false);
                continue;
            }

            const int depth = std::max(1, options.pipeline_depth);
            bool request_failed = false;
            while (static_cast<int>(in_flight.size()) < depth && !paused_flag.load() &&
                   !stop_flag.load()) {
                const int index = claim_chunk(session);
                if (index < 0) {
                    break;
                }

                wire_message request;
                request.id = message_type::REQUEST;
                const uint32_t encoded = encode_chunk_index(index);
                request.payload.resize(sizeof(encoded));
                memcpy(request.payload.data(), &encoded, sizeof(encoded));

                if (!send_wire_message(sock, request)) {
                    release_chunk(index, peer_id, true);
                    failure = "failed to send REQUEST";
                    request_failed = true;
                    break;
                }
                in_flight.push_back(index);
            }

            if (request_failed) {
                break;
            }

            if (in_flight.empty()) {
                if (!work_remaining()) {
                    break;
                }
                // claim_chunk() had nothing for this session while other peers
                // still hold pending chunks. Spinning here forever would keep
                // this session inside the max_active_downloads budget and stop
                // a peer that does own those chunks from ever being dialled, so
                // give the other sessions a bounded number of chances to release
                // a chunk before ending this one.
                if (++unclaimable_rounds >= 40) {
                    break;
                }
                wait_for_work(50);
                continue;
            }
            unclaimable_rounds = 0;

            wire_message message;
            if (!recv_wire_message(sock, message, receive_error)) {
                failed_requests.fetch_add(1);
                failure = "receive failed: " + receive_error;
                break;
            }

            if (message.id == message_type::CHOKE) {
                session->choked.store(true);
                release_all(in_flight, peer_id, true);
                continue;
            }
            if (message.id == message_type::UNCHOKE) {
                session->choked.store(false);
                continue;
            }
            if (message.id == message_type::KEEPALIVE ||
                message.id == message_type::NOT_INTERESTED) {
                continue;
            }
            if (message.id != message_type::PIECE) {
                failure = string("unexpected message: ") + message_type_name(message.id);
                break;
            }

            int index = -1;
            if (!decode_chunk_index(message.payload, index)) {
                failure = "malformed PIECE payload";
                break;
            }
            const auto position = std::find(in_flight.begin(), in_flight.end(), index);
            if (position == in_flight.end()) {
                failure = "received PIECE for a chunk that was not requested";
                break;
            }

            const size_t expected_size = chunk_size_for(index);
            const size_t received_size = message.payload.size() - 4;
            if (expected_size == 0 || received_size != expected_size) {
                failure = "PIECE size does not match the torrent metadata";
                failed_requests.fetch_add(1);
                break;
            }

            const string data(reinterpret_cast<const char*>(message.payload.data() + 4),
                              received_size);
            const string hash = expected_hash(index);
            if (!hash.empty()) {
                SHA256 sha;
                if (sha.hash_buffer(data) != hash) {
                    hash_mismatches.fetch_add(1);
                    {
                        lock_guard<mutex> lock(mtx);
                        blacklisted_peers[index].insert(peer_id);
                    }
                    release_chunk(index, peer_id, true);
                    in_flight.erase(position);
                    log.warnning("SHA-256 mismatch for chunk " + to_string(index) +
                                 " from " + peer_id +
                                 "; chunk returned to the queue and peer "
                                 "blacklisted for this chunk");
                    continue;
                }
            }

            if (!file_manager->write_chunk(current_file_path, current_meta.chunk_size,
                                           static_cast<size_t>(index),
                                           vector<char>(data.begin(), data.end()))) {
                release_chunk(index, peer_id, true);
                in_flight.erase(position);
                failure = "failed to persist chunk " + to_string(index);
                break;
            }

            state_manager->mark_chunk_complete(current_file_id, index);
            chunks_verified.fetch_add(1);
            bytes_downloaded.fetch_add(static_cast<uint64_t>(received_size));
            session->chunks_downloaded.fetch_add(1);
            session->bytes_downloaded.fetch_add(static_cast<uint64_t>(received_size));
            in_flight.erase(position);
            complete_chunk(index);

            chunk_verified_callback callback;
            {
                lock_guard<mutex> lock(mtx);
                callback = verified_callback;
            }
            if (callback) {
                callback(current_file_id, index);
            }
        }
    } while (false);

    finish();
}

bool DownloadManager::run_scheduler() {
    Logger log;
    int discovery_failures = 0;
    bool fatal = false;
    string fatal_reason;

    if (!discover_peers()) {
        log.warnning("No peers discovered yet for file " + current_file_id);
    }

    while (!stop_flag.load()) {
        start_available_sessions();

        {
            lock_guard<mutex> lock(mtx);
            if (pending_chunks.empty() && claimed_chunks.empty()) {
                break;
            }
            if (!failed_chunks.empty()) {
                fatal = true;
                fatal_reason = "one or more chunks exhausted their retry budget";
                break;
            }
        }

        if (active_peers.load() == 0) {
            if (work_remaining()) {
                if (discover_peers()) {
                    discovery_failures = 0;
                    continue;
                }
                if (++discovery_failures > options.discovery_attempts) {
                    fatal = true;
                    fatal_reason = "no peer can provide the remaining chunks";
                    break;
                }
                wait_for_work(options.discovery_wait_ms);
                continue;
            }
            break;
        }

        wait_for_work(100);
    }

    const bool cancelled = stop_flag.load();
    stop_flag.store(true);
    cv.notify_all();
    stop_sessions();

    if (cancelled) {
        record_failure("", "download cancelled");
        return false;
    }
    if (fatal) {
        record_failure("", fatal_reason);
        return false;
    }

    bool complete = false;
    {
        lock_guard<mutex> lock(mtx);
        complete = pending_chunks.empty() && claimed_chunks.empty() && failed_chunks.empty();
    }
    if (!complete) {
        record_failure("", "download ended with unfinished chunks");
        return false;
    }

    if (!verify_whole_file()) {
        return false;
    }

    log.success("Download complete and verified for file " + current_file_id);

    // Report which peers actually delivered chunks. Without this a multi-source
    // transfer is indistinguishable from a single-source one in the logs, which
    // is the evidence the demo scenarios need.
    {
        int contributors = 0;
        for (const auto& peer : get_peer_status()) {
            if (peer.chunks_downloaded > 0) {
                log.info("Contributed " + to_string(peer.chunks_downloaded) +
                         " chunk(s) by peer " + peer.peer_id);
                contributors++;
            }
        }
        log.info("Multi-source summary: " + to_string(contributors) +
                 " peer(s) delivered chunks");
    }
    return true;
}

void DownloadManager::stop_sessions() {
    vector<shared_ptr<peer_session>> current;
    {
        lock_guard<mutex> lock(mtx);
        for (auto& entry : sessions) {
            current.push_back(entry.second);
        }
    }

    for (auto& session : current) {
        const socket_t sock = session->sock.load();
        if (net_valid(sock)) {
            net_shutdown(sock);
        }
    }
    for (auto& session : current) {
        if (session->worker.joinable()) {
            session->worker.join();
        }
    }
}

bool DownloadManager::start_download(const torrent_info& meta, const string& file_path,
                                     int num_threads) {
    Logger log;

    if (active_flag.exchange(true)) {
        record_failure("", "another download is already active");
        return false;
    }

    auto release_active = [this](bool result) {
        active_flag.store(false);
        return result;
    };

    if (!file_manager || !state_manager) {
        record_failure("", "download manager requires a file manager and state manager");
        return release_active(false);
    }
    if (meta.file_hash.empty() || meta.num_chunks == 0 || meta.chunk_size == 0 ||
        meta.file_size == 0) {
        record_failure("", "invalid torrent metadata");
        return release_active(false);
    }
    if (!meta.chunk_hashs.empty() &&
        meta.chunk_hashs.size() != meta.num_chunks) {
        record_failure("", "torrent metadata has an inconsistent chunk hash list");
        return release_active(false);
    }

    {
        lock_guard<mutex> lock(mtx);
        current_meta = meta;
        current_file_id = meta.file_hash;
        current_file_path = file_path;
        pending_chunks.clear();
        claimed_chunks.clear();
        failed_chunks.clear();
        chunk_attempts.clear();
        blacklisted_peers.clear();
        peer_failures.clear();
        tried_peers.clear();
        known_peers.clear();
        sessions.clear();
        last_error_text.clear();
        random_first_pending = false;
        download_started = true;
        if (num_threads > 0) {
            max_active_downloads.store(std::max(1, std::min(num_threads, 64)));
        }
    }

    stop_flag.store(false);
    paused_flag.store(false);
    chunks_verified.store(0);
    bytes_downloaded.store(0);
    duplicate_claims.store(0);
    retry_count.store(0);
    failed_requests.store(0);
    hash_mismatches.store(0);
    active_peers.store(0);
    peak_active_peers.store(0);

    log.info("Starting multi-peer download for " + meta.file_name + " (" +
             to_string(meta.num_chunks) + " chunks)");

    if (!file_manager->ensure_file_size(file_path, meta.file_size)) {
        record_failure("", "cannot prepare the destination file");
        return release_active(false);
    }

    state_manager->init_file_state(meta.file_hash, meta.num_chunks, false);

    if (options.verify_existing_chunks && !verify_existing_chunks()) {
        record_failure("", "failed to verify persisted chunks");
        return release_active(false);
    }

    if (state_manager->is_completed(meta.file_hash)) {
        if (verify_whole_file()) {
            log.success("File is already complete and verified: " + meta.file_name);
            return release_active(true);
        }
        record_failure("", "completed state on disk does not match the torrent hash");
        return release_active(false);
    }

    {
        lock_guard<mutex> lock(mtx);
        const vector<int> missing = state_manager->get_missing_chunks(meta.file_hash);
        for (int index : missing) {
            pending_chunks.insert(index);
        }
        random_first_pending = state_manager->get_completed_chunks_count(meta.file_hash) == 0;
    }

    if (!work_remaining()) {
        if (verify_whole_file()) {
            return release_active(true);
        }
        return release_active(false);
    }

    const bool result = run_scheduler();
    return release_active(result);
}

vector<peer_transfer_status> DownloadManager::get_peer_status() const {
    lock_guard<mutex> lock(mtx);
    vector<peer_transfer_status> result;
    result.reserve(sessions.size());
    for (const auto& entry : sessions) {
        const auto& session = entry.second;
        peer_transfer_status status;
        status.peer_id = session->peer.peer_id;
        status.ip = session->peer.ip;
        status.port = session->peer.port;
        status.connected = session->connected.load();
        status.choked = session->choked.load();
        status.chunks_downloaded = session->chunks_downloaded.load();
        status.bytes_downloaded = session->bytes_downloaded.load();
        status.last_error = session->last_error;
        result.push_back(status);
    }
    return result;
}

download_stats DownloadManager::get_stats() const {
    download_stats snapshot;
    snapshot.chunks_verified = chunks_verified.load();
    snapshot.bytes_downloaded = bytes_downloaded.load();
    snapshot.duplicate_claims = duplicate_claims.load();
    snapshot.retries = retry_count.load();
    snapshot.failed_requests = failed_requests.load();
    snapshot.hash_mismatches = hash_mismatches.load();
    snapshot.active_peers = active_peers.load();
    snapshot.peak_active_peers = peak_active_peers.load();
    return snapshot;
}

bool DownloadManager::download_chunk_from_peer(const peer_info_response& peer,
                                               const string& file_id, int chunk_idx,
                                               size_t chunk_size,
                                               const string& expected_hash_value,
                                               const string& file_path) {
    Logger log;
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (!net_valid(sock)) {
        log.err("Cannot create client socket for peer " + peer.peer_id);
        return false;
    }

    auto close_socket = [&]() {
        net_shutdown(sock);
        net_close(sock);
    };

    net_set_timeouts(sock, options.io_timeout_ms);

    struct sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<uint16_t>(peer.port));
    if (inet_pton(AF_INET, peer.ip.c_str(), &server_addr.sin_addr) != 1) {
        log.err("Invalid IP address: " + peer.ip);
        close_socket();
        return false;
    }

    if (!net_connect_timeout(sock, reinterpret_cast<sockaddr*>(&server_addr),
                             sizeof(server_addr), options.connect_timeout_ms)) {
        log.err("Failed to connect to peer " + peer.peer_id);
        close_socket();
        return false;
    }

    wire_message handshake;
    handshake.id = message_type::HANDSHAKE;
    handshake.payload = encode_handshake({my_peer_id, file_id});
    if (!send_wire_message(sock, handshake)) {
        close_socket();
        return false;
    }

    string error;
    wire_message message;
    if (!recv_wire_message(sock, message, error) ||
        message.id != message_type::HANDSHAKE) {
        close_socket();
        return false;
    }
    if (!recv_wire_message(sock, message, error) ||
        message.id != message_type::BITFIELD) {
        close_socket();
        return false;
    }

    wire_message interested;
    interested.id = message_type::INTERESTED;
    if (!send_wire_message(sock, interested)) {
        close_socket();
        return false;
    }

    bool unchoked = false;
    for (int i = 0; i < 4 && !unchoked; ++i) {
        if (!recv_wire_message(sock, message, error)) {
            close_socket();
            return false;
        }
        if (message.id == message_type::UNCHOKE) {
            unchoked = true;
        }
    }
    if (!unchoked) {
        close_socket();
        return false;
    }

    wire_message request;
    request.id = message_type::REQUEST;
    const uint32_t encoded = encode_chunk_index(chunk_idx);
    request.payload.resize(sizeof(encoded));
    memcpy(request.payload.data(), &encoded, sizeof(encoded));
    if (!send_wire_message(sock, request)) {
        close_socket();
        return false;
    }

    if (!recv_wire_message(sock, message, error) || message.id != message_type::PIECE) {
        close_socket();
        return false;
    }

    int received_index = -1;
    if (!decode_chunk_index(message.payload, received_index) ||
        received_index != chunk_idx) {
        close_socket();
        return false;
    }

    const string data(reinterpret_cast<const char*>(message.payload.data() + 4),
                      message.payload.size() - 4);
    close_socket();

    if (!expected_hash_value.empty()) {
        SHA256 sha;
        if (sha.hash_buffer(data) != expected_hash_value) {
            log.err("Hash mismatch for chunk " + to_string(chunk_idx));
            return false;
        }
    }

    if (file_manager &&
        !file_manager->write_chunk(file_path, chunk_size,
                                   static_cast<size_t>(chunk_idx),
                                   vector<char>(data.begin(), data.end()))) {
        return false;
    }
    if (state_manager) {
        state_manager->mark_chunk_complete(file_id, chunk_idx);
    }
    return true;
}
