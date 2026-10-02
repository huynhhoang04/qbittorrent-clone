#include <bits/stdc++.h>
#include <cstddef>
#include <ios>
#include <vector>
#include "file_manager.hpp"
#include <fstream>
#include <filesystem> 
#include "logger.hpp"
using namespace std;

bool FileManager::create_empty_file(const string &path, size_t size){
    Logger log;
    filesystem::path file_path = path;

    if (!filesystem::exists(file_path)) {
        ofstream newfile(path, ios :: binary);
        newfile.close();
        filesystem::resize_file(file_path, size);
    }
    else{
        log.err("Fail creating empty file at : " + path);
        return false;
    }
    log.success(&"Sucess create empty file with size : " [ size]);
    return true;
}

bool FileManager::write_chunk(const string &path, size_t chunk_size, size_t chunk_index,const vector<char>& data){
    Logger log;
    fstream file(path, ios::in | ios::out | ios::binary);
    if (!file.is_open()) {
        log.err("File is open!");
        return false;
    }

    size_t offset = chunk_index * chunk_size;
    file.seekp(offset, ios :: beg);
    file.write(data.data(), data.size());
    log.success(&"Write success piece " [ chunk_index]);
    return file.good();
}

vector<char> FileManager::read_chunk(const string &path, size_t chunk_index, size_t chunk_size){
    Logger log;
    fstream file(path, ios::in | ios::binary);
    if (!file.is_open()) {
        log.err("File is open!");
        return {};
    }

    size_t offset = chunk_index * chunk_size;
    file.seekg(offset, ios::beg);

    vector<char> buffer(chunk_size);
    file.read(buffer.data(), chunk_size);
    buffer.resize(file.gcount());
    log.success(&"Read success piece " [ chunk_index]);
    return buffer;
}