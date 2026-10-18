#pragma once
#include <bits/stdc++.h>
#include "protocol.hpp"

using namespace std;

struct peer_entry {
    string peer_id;
    string ip;
    int port;
    vector<bool> bitfield;
    chrono::steady_clock::time_point last_seen;
};

void add_or_update(const string& file_id, const peer_entry& peer);
vector<peer_entry> get_peers(const string& file_id);
void cleanup_stale(int timeout_seconds = 45);
size_t total_peers();

class PeerRegistry {
    private:
        unordered_map<string, vector<peer_entry>> registry;
        mutex mtx;

    public:
        void add_or_update(const string& file_id, const peer_entry& peer);
        vector<peer_entry> get_peers(const string& file_id);
        void cleanup_stale(int timeout_seconds = 45);
        size_t total_peers();
        void clear();
};
