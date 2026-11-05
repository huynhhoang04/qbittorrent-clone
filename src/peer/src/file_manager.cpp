#include "file_manager.hpp"
#include "logger.hpp"

#include <filesystem>
#include <fstream>
#include <limits>

using namespace std;

namespace {

bool write_chunk_unlocked(const string& path, size_t chunk_size, size_t chunk_index,
                          const vector<char>& data, Logger& log) {
    if (path.empty() || chunk_size == 0 || data.empty()) {
        log.err("Refusing to write an empty or invalid chunk");
        return false;
    }
    if (chunk_index > (numeric_limits<size_t>::max() / chunk_size)) {
        log.err("Chunk offset overflow for chunk " + to_string(chunk_index));
        return false;
    }

    fstream file(path, ios::in | ios::out | ios::binary);
    if (!file.is_open()) {
        log.err("Cannot open file for writing: " + path);
        return false;
    }

    const size_t offset = chunk_index * chunk_size;
    file.seekp(static_cast<streamoff>(offset), ios::beg);
    if (!file.good()) {
        log.err("Cannot seek to chunk offset in " + path);
        return false;
    }

    file.write(data.data(), static_cast<streamsize>(data.size()));
    file.flush();
    if (!file.good()) {
        log.err("Short write for chunk " + to_string(chunk_index) + " in " + path);
        return false;
    }
    return true;
}

} // namespace

bool create_empty_file(const string& path, size_t size) {
    FileManager manager;
    return manager.create_empty_file(path, size);
}

bool write_chunk(const string& path, size_t chunk_size, size_t chunk_index,
                 const vector<char>& data) {
    FileManager manager;
    return manager.write_chunk(path, chunk_size, chunk_index, data);
}

vector<char> read_chunk(const string& path, size_t chunk_index, size_t chunk_size) {
    FileManager manager;
    return manager.read_chunk(path, chunk_index, chunk_size);
}

bool FileManager::create_empty_file(const string& path, size_t size) {
    Logger log;
    lock_guard<mutex> lock(file_mtx);

    filesystem::path file_path = path;
    if (filesystem::exists(file_path)) {
        log.err("File already exists: " + path);
        return false;
    }

    ofstream new_file(path, ios::binary);
    if (!new_file.is_open()) {
        log.err("Cannot create file: " + path);
        return false;
    }
    new_file.close();

    error_code ec;
    filesystem::resize_file(file_path, size, ec);
    if (ec) {
        log.err("Cannot resize file " + path + ": " + ec.message());
        return false;
    }

    log.success("Created file " + path + " (" + to_string(size) + " bytes)");
    return true;
}

bool FileManager::ensure_file_size(const string& path, size_t size) {
    Logger log;
    lock_guard<mutex> lock(file_mtx);

    error_code ec;
    if (!filesystem::exists(path)) {
        ofstream new_file(path, ios::binary);
        if (!new_file.is_open()) {
            log.err("Cannot create file: " + path);
            return false;
        }
        new_file.close();
        filesystem::resize_file(path, size, ec);
        if (ec) {
            log.err("Cannot resize file " + path + ": " + ec.message());
            return false;
        }
        return true;
    }

    const auto current_size = filesystem::file_size(path, ec);
    if (ec) {
        log.err("Cannot inspect file " + path + ": " + ec.message());
        return false;
    }
    if (current_size != size) {
        filesystem::resize_file(path, size, ec);
        if (ec) {
            log.err("Cannot resize existing file " + path + ": " + ec.message());
            return false;
        }
    }
    return true;
}

bool FileManager::write_chunk(const string& path, size_t chunk_size, size_t chunk_index,
                              const vector<char>& data) {
    Logger log;
    lock_guard<mutex> lock(file_mtx);
    return write_chunk_unlocked(path, chunk_size, chunk_index, data, log);
}

vector<char> FileManager::read_chunk(const string& path, size_t chunk_index,
                                     size_t chunk_size) {
    Logger log;
    lock_guard<mutex> lock(file_mtx);

    if (path.empty() || chunk_size == 0) {
        return {};
    }

    fstream file(path, ios::in | ios::binary);
    if (!file.is_open()) {
        log.err("Cannot open file for reading: " + path);
        return {};
    }

    if (chunk_index > (numeric_limits<size_t>::max() / chunk_size)) {
        return {};
    }
    const size_t offset = chunk_index * chunk_size;
    file.seekg(static_cast<streamoff>(offset), ios::beg);
    if (!file.good()) {
        return {};
    }

    vector<char> buffer(chunk_size);
    file.read(buffer.data(), static_cast<streamsize>(chunk_size));
    buffer.resize(static_cast<size_t>(file.gcount()));
    return buffer;
}
