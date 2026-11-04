#pragma once

#include <bits/stdc++.h>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

using namespace std;

bool create_empty_file(const string& path, size_t size);
bool write_chunk(const string& path, size_t chunk_size, size_t chunk_index,
                 const vector<char>& data);
vector<char> read_chunk(const string& path, size_t chunk_index, size_t chunk_size);

class FileManager {
    public:
        bool create_empty_file(const string& path, size_t size);

        // Creates the file when missing, otherwise makes sure the size matches
        // the torrent. Used both by fresh downloads and crash recovery.
        bool ensure_file_size(const string& path, size_t size);

        bool write_chunk(const string& path, size_t chunk_size, size_t chunk_index,
                         const vector<char>& data);
        vector<char> read_chunk(const string& path, size_t chunk_index, size_t chunk_size);

    private:
        mutex file_mtx;
};
