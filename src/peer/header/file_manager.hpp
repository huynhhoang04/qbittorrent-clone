#pragma once
#include <bits/stdc++.h>
#include <cstddef>
#include <string>
#include <vector>
using namespace std;

bool create_empty_file(const string& path, size_t size);
bool write_chunk(const string& path, size_t chunkz_size, size_t chunk_index,const vector<char>& data);
vector<char> read_chunk(const string& path, size_t chunk_index, size_t chunk_size);

class FileManager{
    public:
        bool create_empty_file(const string& path, size_t size);
        bool write_chunk(const string& path, size_t chunkz_size, size_t chunk_index,const vector<char>& data);
        vector<char> read_chunk(const string& path, size_t chunk_index, size_t chunk_size);
};