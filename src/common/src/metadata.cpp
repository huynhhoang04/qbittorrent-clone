#include <bits/stdc++.h>
#include "metadata.hpp"
#include "logger.hpp"
#include "sha256.hpp"
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <ios>
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <nlohmann/json_fwd.hpp>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;
using json = nlohmann::json;

bool Metadata::generate_torrent(const string &input_path, size_t chunk_size, const string &output_path, const string& tracker_url){
    Logger log;
    torrent_info info;
    ifstream file(input_path, ios_base::binary);
    if (!file.is_open()) {
        log.err("Cannot open input file");
        return false;
    }
    SHA256 sha;
    info.file_name = filesystem::path(input_path).filename();
    info.file_size = filesystem::file_size(input_path);
    info.chunk_size = chunk_size;
    info.file_hash = sha.hash_file(input_path);
    info.tracker_url = tracker_url;
    info.num_chunks = ceil((double)info.file_size/chunk_size);
    vector<string> chunk_hashs;
    vector<char> hash_buffer(chunk_size);
    while (file) {
        file.read(hash_buffer.data(), chunk_size);
        streamsize bytesread = file.gcount();
        if (bytesread > 0) {
            chunk_hashs.push_back(sha.hash_buffer(string(hash_buffer.data(), bytesread)));
        }
    }

    info.chunk_hashs = chunk_hashs;

    json j = info;
    ofstream output(output_path);
    if (output.is_open()){
        output << setw(4) << j << endl;
        output.close();
        log.success("Suceess create torrent file at : " + output_path);
        return true;
    }
    log.err("Fail creating torrent file");
    return false;
}

torrent_info Metadata::load(const string& torrent_path){
    Logger log;
    ifstream file(torrent_path);
    if (file.is_open()) {
        json j;
        file >> j;

        torrent_info info = j.get<torrent_info>();
        file.close();
        log.success("Return torrrent info now");
        return info;
    }
    else {
        log.err("Fail reading torrent");
        throw invalid_argument("Some thing wrong!");
    }
}