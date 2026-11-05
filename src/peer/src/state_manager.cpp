#include "state_manager.hpp"
#include "logger.hpp"

using namespace std;

StateManager::StateManager(Database* db) : db(db) {
    if (db) {
        load_from_db();
    }
}

void StateManager::load_from_db() {
    if (!db) return;

    Logger log;
    lock_guard<mutex> lock(mtx);
    auto torrents = db->get_all_torrents();
    for (const auto& t : torrents) {
        vector<bool> bf(t.num_chunks, false);
        auto chunks = db->get_chunks(t.file_id);
        for (const auto& c : chunks) {
            if (c.chunk_index >= 0 && (size_t)c.chunk_index < t.num_chunks) {
                if (c.status == "verified") {
                    bf[c.chunk_index] = true;
                }
            }
        }
        bitfields[t.file_id] = bf;
    }
    log.info("Loaded state for " + to_string(torrents.size()) + " torrent(s) from database");
}

void StateManager::init_file_state(const string& file_id, size_t num_chunks,
                                   bool is_seeder) {
    Logger log;
    lock_guard<mutex> lock(mtx);

    // Preserve persisted progress when the torrent is already tracked.
    auto it = bitfields.find(file_id);
    if (it != bitfields.end() && it->second.size() == num_chunks) {
        return;
    }

    vector<bool> bf(num_chunks, is_seeder);
    bitfields[file_id] = bf;

    if (db) {
        for (size_t i = 0; i < num_chunks; ++i) {
            chunk_record cr;
            cr.file_id = file_id;
            cr.chunk_index = static_cast<int>(i);
            cr.hash = "";
            cr.status = is_seeder ? "verified" : "missing";
            db->upsert_chunk(cr);
        }
    }

    log.info("Initialized state for file " + file_id + " (" +
             to_string(num_chunks) + " chunks)");
}

vector<bool> StateManager::get_bitfield(const string& file_id) {
    lock_guard<mutex> lock(mtx);
    auto it = bitfields.find(file_id);
    if (it != bitfields.end()) {
        return it->second;
    }
    return {};
}

bool StateManager::has_chunk(const string& file_id, int chunk_index) {
    lock_guard<mutex> lock(mtx);
    auto it = bitfields.find(file_id);
    if (it != bitfields.end()) {
        if (chunk_index >= 0 && (size_t)chunk_index < it->second.size()) {
            return it->second[chunk_index];
        }
    }
    return false;
}

void StateManager::mark_chunk_complete(const string& file_id, int chunk_index) {
    Logger log;
    lock_guard<mutex> lock(mtx);

    auto it = bitfields.find(file_id);
    if (it != bitfields.end()) {
        if (chunk_index >= 0 && (size_t)chunk_index < it->second.size()) {
            it->second[chunk_index] = true;
            if (db) {
                db->update_chunk_status(file_id, chunk_index, "verified");
            }
            log.success("Verified chunk " + to_string(chunk_index) + " for file " + file_id);
        }
    }
}

void StateManager::mark_all_complete(const string& file_id) {
    Logger log;
    lock_guard<mutex> lock(mtx);

    auto it = bitfields.find(file_id);
    if (it == bitfields.end()) {
        return;
    }
    for (size_t i = 0; i < it->second.size(); ++i) {
        it->second[i] = true;
        if (db) {
            db->update_chunk_status(file_id, static_cast<int>(i), "verified");
        }
    }
    log.info("Marked all chunks verified for file " + file_id);
}

void StateManager::mark_chunk_missing(const string& file_id, int chunk_index) {    Logger log;
    lock_guard<mutex> lock(mtx);

    auto it = bitfields.find(file_id);
    if (it != bitfields.end()) {
        if (chunk_index >= 0 && (size_t)chunk_index < it->second.size()) {
            it->second[chunk_index] = false;
            if (db) {
                db->update_chunk_status(file_id, chunk_index, "missing");
            }
            log.warnning("Marked chunk " + to_string(chunk_index) + " as missing for file " +
                         file_id);
        }
    }
}

bool StateManager::is_completed(const string& file_id) {
    lock_guard<mutex> lock(mtx);
    auto it = bitfields.find(file_id);
    if (it == bitfields.end() || it->second.empty()) {
        return false;
    }
    for (bool chunk_done : it->second) {
        if (!chunk_done) {
            return false;
        }
    }
    return true;
}

vector<int> StateManager::get_missing_chunks(const string& file_id) {
    lock_guard<mutex> lock(mtx);
    vector<int> missing;
    auto it = bitfields.find(file_id);
    if (it != bitfields.end()) {
        for (size_t i = 0; i < it->second.size(); ++i) {
            if (!it->second[i]) {
                missing.push_back(static_cast<int>(i));
            }
        }
    }
    return missing;
}

vector<int> StateManager::get_verified_chunks(const string& file_id) {
    lock_guard<mutex> lock(mtx);
    vector<int> verified;
    auto it = bitfields.find(file_id);
    if (it != bitfields.end()) {
        for (size_t i = 0; i < it->second.size(); ++i) {
            if (it->second[i]) {
                verified.push_back(static_cast<int>(i));
            }
        }
    }
    return verified;
}

size_t StateManager::get_num_chunks(const string& file_id) {
    lock_guard<mutex> lock(mtx);
    auto it = bitfields.find(file_id);
    return it != bitfields.end() ? it->second.size() : 0;
}

double StateManager::get_progress(const string& file_id) {
    lock_guard<mutex> lock(mtx);
    auto it = bitfields.find(file_id);
    if (it == bitfields.end() || it->second.empty()) {
        return 0.0;
    }
    size_t done = 0;
    for (bool b : it->second) {
        if (b) done++;
    }
    return ((double)done / (double)it->second.size()) * 100.0;
}

size_t StateManager::get_completed_chunks_count(const string& file_id) {
    lock_guard<mutex> lock(mtx);
    auto it = bitfields.find(file_id);
    if (it == bitfields.end()) return 0;
    size_t count = 0;
    for (bool b : it->second) {
        if (b) count++;
    }
    return count;
}
