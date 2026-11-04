#pragma once

#include <bits/stdc++.h>
#include "database.hpp"

using namespace std;

class StateManager {
    private:
        Database* db;
        unordered_map<string, vector<bool>> bitfields;
        mutex mtx;

    public:
        StateManager(Database* db = nullptr);

        void load_from_db();
        void init_file_state(const string& file_id, size_t num_chunks,
                             bool is_seeder = false);
        vector<bool> get_bitfield(const string& file_id);
        bool has_chunk(const string& file_id, int chunk_index);
        void mark_chunk_complete(const string& file_id, int chunk_index);
        void mark_chunk_missing(const string& file_id, int chunk_index);
        void mark_all_complete(const string& file_id);
        bool is_completed(const string& file_id);
        vector<int> get_missing_chunks(const string& file_id);
        vector<int> get_verified_chunks(const string& file_id);
        size_t get_num_chunks(const string& file_id);
        double get_progress(const string& file_id);
        size_t get_completed_chunks_count(const string& file_id);
};
