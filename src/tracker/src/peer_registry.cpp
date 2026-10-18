#include <bits/stdc++.h>
#include "peer_registry.hpp"
#include "logger.hpp"

using namespace std;

static PeerRegistry default_registry;

void add_or_update(const string& file_id, const peer_entry& peer) {
    default_registry.add_or_update(file_id, peer);
}

vector<peer_entry> get_peers(const string& file_id) {
    return default_registry.get_peers(file_id);
}

void cleanup_stale(int timeout_seconds) {
    default_registry.cleanup_stale(timeout_seconds);
}

size_t total_peers() {
    return default_registry.total_peers();
}

void PeerRegistry::add_or_update(const string& file_id, const peer_entry& peer) {
    Logger log;
    lock_guard<mutex> lock(mtx);

    auto now = chrono::steady_clock::now();
    auto effective_time = (peer.last_seen == chrono::steady_clock::time_point{}) ? now : peer.last_seen;

    auto& peers = registry[file_id];
    for (auto& p : peers) {
        if (p.peer_id == peer.peer_id) {
            p.ip = peer.ip;
            p.port = peer.port;
            p.bitfield = peer.bitfield;
            p.last_seen = effective_time;
            log.info("Updated peer " + peer.peer_id + " for file " + file_id);
            return;
        }
    }

    peer_entry entry = peer;
    entry.last_seen = effective_time;
    peers.push_back(entry);
    log.success("Added peer " + peer.peer_id + " for file " + file_id);
}

vector<peer_entry> PeerRegistry::get_peers(const string& file_id) {
    Logger log;
    lock_guard<mutex> lock(mtx);

    auto it = registry.find(file_id);
    if (it != registry.end()) {
        log.info("Found " + to_string(it->second.size()) + " peers for file " + file_id);
        return it->second;
    }
    return {};
}

void PeerRegistry::cleanup_stale(int timeout_seconds) {
    Logger log;
    lock_guard<mutex> lock(mtx);

    auto now = chrono::steady_clock::now();
    auto timeout = chrono::seconds(timeout_seconds);

    for (auto it = registry.begin(); it != registry.end(); ) {
        auto& peers = it->second;
        peers.erase(
            remove_if(peers.begin(), peers.end(), [&](const peer_entry& p) {
                return (now - p.last_seen) > timeout;
            }),
            peers.end()
        );

        if (peers.empty()) {
            it = registry.erase(it);
        } else {
            ++it;
        }
    }
}

size_t PeerRegistry::total_peers() {
    lock_guard<mutex> lock(mtx);

    unordered_set<string> unique_peers;
    for (const auto& [fid, peers] : registry) {
        for (const auto& p : peers) {
            unique_peers.insert(p.peer_id);
        }
    }
    return unique_peers.size();
}

void PeerRegistry::clear() {
    lock_guard<mutex> lock(mtx);
    registry.clear();
}
