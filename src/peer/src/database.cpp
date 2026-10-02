#include "database.hpp"
#include <iostream>

using namespace std;

Database::Database(const string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        string err = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        throw runtime_error("Failed to open database: " + err);
    }
    // Kích hoạt chế độ WAL cho hiệu năng cao hơn khi đa luồng
    execute("PRAGMA journal_mode=WAL;");
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void Database::execute(const string& sql) {
    char* err_msg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg) != SQLITE_OK) {
        string err = err_msg;
        sqlite3_free(err_msg);
        throw runtime_error("SQL error: " + err);
    }
}

void Database::init_schema() {
    lock_guard<mutex> lock(db_mutex_);
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS torrents (
            file_id      TEXT PRIMARY KEY,
            file_name    TEXT NOT NULL,
            file_size    INTEGER NOT NULL,
            chunk_size   INTEGER NOT NULL,
            num_chunks   INTEGER NOT NULL,
            file_hash    TEXT NOT NULL,
            status       TEXT NOT NULL DEFAULT 'downloading',
            metadata_path TEXT NOT NULL,
            output_path  TEXT NOT NULL,
            uploaded     INTEGER DEFAULT 0,
            downloaded   INTEGER DEFAULT 0,
            added_at     DATETIME DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS chunks (
            file_id      TEXT NOT NULL,
            chunk_index  INTEGER NOT NULL,
            hash         TEXT NOT NULL,
            status       TEXT NOT NULL DEFAULT 'missing',
            PRIMARY KEY (file_id, chunk_index),
            FOREIGN KEY (file_id) REFERENCES torrents(file_id)
        );

        CREATE TABLE IF NOT EXISTS peer_history (
            peer_id      TEXT NOT NULL,
            ip           TEXT NOT NULL,
            port         INTEGER NOT NULL,
            file_id      TEXT NOT NULL,
            last_seen    DATETIME DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (peer_id, file_id),
            FOREIGN KEY (file_id) REFERENCES torrents(file_id)
        );
    )";
    execute(schema);
}

void Database::upsert_torrent(const torrent_record& tr) {
    lock_guard<mutex> lock(db_mutex_);
    // Using prepared statement for safety
    string sql = "INSERT OR REPLACE INTO torrents (file_id, file_name, file_size, chunk_size, num_chunks, file_hash, status, metadata_path, output_path, uploaded, downloaded) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, tr.file_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, tr.file_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, tr.file_size);
        sqlite3_bind_int64(stmt, 4, tr.chunk_size);
        sqlite3_bind_int64(stmt, 5, tr.num_chunks);
        sqlite3_bind_text(stmt, 6, tr.file_hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, tr.status.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 8, tr.metadata_path.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 9, tr.output_path.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 10, tr.uploaded);
        sqlite3_bind_int64(stmt, 11, tr.downloaded);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

vector<torrent_record> Database::get_all_torrents() {
    lock_guard<mutex> lock(db_mutex_);
    vector<torrent_record> results;
    const char* sql = "SELECT file_id, file_name, file_size, chunk_size, num_chunks, file_hash, status, metadata_path, output_path, uploaded, downloaded, added_at FROM torrents;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            torrent_record tr;
            tr.file_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            tr.file_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            tr.file_size = sqlite3_column_int64(stmt, 2);
            tr.chunk_size = sqlite3_column_int64(stmt, 3);
            tr.num_chunks = sqlite3_column_int64(stmt, 4);
            tr.file_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            tr.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            tr.metadata_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            tr.output_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            tr.uploaded = sqlite3_column_int64(stmt, 9);
            tr.downloaded = sqlite3_column_int64(stmt, 10);
            
            const char* added = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
            if (added) tr.added_at = added;
            
            results.push_back(tr);
        }
        sqlite3_finalize(stmt);
    }
    return results;
}

void Database::upsert_chunk(const chunk_record& cr) {
    lock_guard<mutex> lock(db_mutex_);
    string sql = "INSERT OR REPLACE INTO chunks (file_id, chunk_index, hash, status) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, cr.file_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, cr.chunk_index);
        sqlite3_bind_text(stmt, 3, cr.hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, cr.status.c_str(), -1, SQLITE_TRANSIENT);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void Database::update_chunk_status(const string& file_id, int chunk_index, const string& status) {
    lock_guard<mutex> lock(db_mutex_);
    string sql = "UPDATE chunks SET status = ? WHERE file_id = ? AND chunk_index = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, file_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, chunk_index);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

vector<chunk_record> Database::get_chunks(const string& file_id) {
    lock_guard<mutex> lock(db_mutex_);
    vector<chunk_record> results;
    string sql = "SELECT chunk_index, hash, status FROM chunks WHERE file_id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, file_id.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            chunk_record cr;
            cr.file_id = file_id;
            cr.chunk_index = sqlite3_column_int(stmt, 0);
            cr.hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            cr.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            results.push_back(cr);
        }
        sqlite3_finalize(stmt);
    }
    return results;
}

void Database::upsert_peer_history(const peer_history_record& ph) {
    lock_guard<mutex> lock(db_mutex_);
    string sql = "INSERT OR REPLACE INTO peer_history (peer_id, ip, port, file_id, last_seen) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ph.peer_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, ph.ip.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, ph.port);
        sqlite3_bind_text(stmt, 4, ph.file_id.c_str(), -1, SQLITE_TRANSIENT);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

vector<peer_history_record> Database::get_peers_for_file(const string& file_id) {
    lock_guard<mutex> lock(db_mutex_);
    vector<peer_history_record> results;
    string sql = "SELECT peer_id, ip, port, last_seen FROM peer_history WHERE file_id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, file_id.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            peer_history_record ph;
            ph.file_id = file_id;
            ph.peer_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            ph.ip = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            ph.port = sqlite3_column_int(stmt, 2);
            
            const char* seen = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            if (seen) ph.last_seen = seen;
            
            results.push_back(ph);
        }
        sqlite3_finalize(stmt);
    }
    return results;
}
