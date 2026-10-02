#include <cstddef>
#include <nlohmann/detail/macro_scope.hpp>
#include <string>
#include <vector>
#pragma once
#include <bits/stdc++.h>
#include <nlohmann/json.hpp>
using namespace std; 

struct torrent_info{
    string file_name;
    size_t file_size;
    size_t chunk_size;
    size_t num_chunks;
    string file_hash;
    string tracker_url;
    vector<string> chunk_hashs;
};

bool generate_torrent(const string& input_path, size_t chunk_size, const string& output_path, const string& tracker_url);
torrent_info load(const string& torrent_path);

class Metadata{
    public:
        bool generate_torrent(const string& input_path, size_t chunk_size, const string& output_path, const string& tracker_url);
        torrent_info load(const string& torrent_path);
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(torrent_info, file_name, file_size, chunk_size, num_chunks, file_hash, tracker_url, chunk_hashs);