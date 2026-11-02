#include <gtest/gtest.h>
#include "state_manager.hpp"
#include "logger.hpp"
#include <filesystem>

using namespace std;

TEST(StateManagerTest, InitAndProgress) {
    Logger log;
    log.info("Chạy bài test StateManagerTest.InitAndProgress...");

    string db_path = "test_state.db";
    if (filesystem::exists(db_path)) {
        filesystem::remove(db_path);
    }

    Database db(db_path);
    db.init_schema();

    StateManager sm(&db);
    string file_id = "test-hash-123";
    size_t num_chunks = 4;

    sm.init_file_state(file_id, num_chunks, false);

    EXPECT_FALSE(sm.is_completed(file_id));
    EXPECT_DOUBLE_EQ(sm.get_progress(file_id), 0.0);

    auto missing = sm.get_missing_chunks(file_id);
    ASSERT_EQ(missing.size(), 4);

    // Đánh dấu chunk 0 và 2 hoàn thành
    sm.mark_chunk_complete(file_id, 0);
    sm.mark_chunk_complete(file_id, 2);

    EXPECT_TRUE(sm.has_chunk(file_id, 0));
    EXPECT_FALSE(sm.has_chunk(file_id, 1));
    EXPECT_TRUE(sm.has_chunk(file_id, 2));
    EXPECT_DOUBLE_EQ(sm.get_progress(file_id), 50.0);

    // Hoàn thành nốt chunk 1 và 3
    sm.mark_chunk_complete(file_id, 1);
    sm.mark_chunk_complete(file_id, 3);

    EXPECT_TRUE(sm.is_completed(file_id));
    EXPECT_DOUBLE_EQ(sm.get_progress(file_id), 100.0);
    EXPECT_TRUE(sm.get_missing_chunks(file_id).empty());

    if (filesystem::exists(db_path)) {
        filesystem::remove(db_path);
    }
}
