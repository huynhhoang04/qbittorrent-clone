#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <sqlite3.h>
#include <stdexcept>

using namespace std;

struct torrent_record {
    string file_id;
    string file_name;
    size_t file_size;
    size_t chunk_size;
    size_t num_chunks;
    string file_hash;
    string status;
    string metadata_path;
    string output_path;
    size_t uploaded;
    size_t downloaded;
    string added_at;
};

struct chunk_record {
    string file_id;
    int chunk_index;
    string hash;
    string status;
};

struct peer_history_record {
    string peer_id;
    string ip;
    int port;
    string file_id;
    string last_seen;
};

class Database {
public:
    Database(const string& db_path);
    ~Database();

    // Khởi tạo bảng
    void init_schema();

    // Thao tác với Torrents
    void upsert_torrent(const torrent_record& tr);
    vector<torrent_record> get_all_torrents();

    // Thao tác với Chunks
    void upsert_chunk(const chunk_record& cr);
    void update_chunk_status(const string& file_id, int chunk_index, const string& status);
    vector<chunk_record> get_chunks(const string& file_id);

    // Thao tác với Peer History
    void upsert_peer_history(const peer_history_record& ph);
    vector<peer_history_record> get_peers_for_file(const string& file_id);

private:
    void execute(const string& sql);

    sqlite3* db_ = nullptr;
    mutex db_mutex_;
};
