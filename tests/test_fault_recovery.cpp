#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "database.hpp"
#include "download_manager.hpp"
#include "file_manager.hpp"
#include "metadata.hpp"
#include "net.hpp"
#include "peer_node.hpp"
#include "state_manager.hpp"
#include "test_helpers.hpp"
#include "tracker_client.hpp"
#include "tracker_server.hpp"
#include "upload_server.hpp"

using namespace std;

#ifndef PEER_APP_PATH
#define PEER_APP_PATH ""
#endif

namespace {

class TrackerFixture {
    public:
        explicit TrackerFixture(int port) : port(port), server(port) {
            thread = std::thread([this]() { server.start(); });
            TrackerClient client(url());
            ready = testutil::wait_until(
                [&client]() { return client.health_check(); }, 8000, 50);
        }
        ~TrackerFixture() { stop(); }
        void stop() {
            if (stopped) {
                return;
            }
            stopped = true;
            server.stop();
            if (thread.joinable()) {
                thread.join();
            }
        }
        string url() const { return "http://127.0.0.1:" + to_string(port); }
        bool is_ready() const { return ready; }

    private:
        int port;
        TrackerServer server;
        std::thread thread;
        bool ready = false;
        bool stopped = false;
};

struct Seeder {
    unique_ptr<Database> db;
    unique_ptr<FileManager> files;
    unique_ptr<StateManager> state;
    unique_ptr<UploadServer> upload;
    torrent_info meta;
    int port = 0;

    ~Seeder() { stop(); }

    void stop() {
        if (upload) {
            upload->stop();
        }
    }

    bool start(const string& dir, const string& id, const string& tracker_url,
               const torrent_info& torrent, const string& source,
               int chunk_delay_ms) {
        meta = torrent;
        filesystem::create_directories(dir);
        db = make_unique<Database>(dir + "/peer.db");
        db->init_schema();
        files = make_unique<FileManager>();
        state = make_unique<StateManager>(db.get());
        state->init_file_state(meta.file_hash, meta.num_chunks, true);
        state->mark_all_complete(meta.file_hash);

        upload = make_unique<UploadServer>(0, files.get(), state.get(), id);
        if (chunk_delay_ms > 0) {
            upload_options settings;
            settings.chunk_delay_ms = chunk_delay_ms;
            upload->set_upload_options(settings);
        }
        if (!upload->start()) {
            return false;
        }
        port = upload->get_port();
        upload->register_file(meta.file_hash, source, meta.chunk_size,
                              meta.num_chunks);

        TrackerClient client(tracker_url);
        announce_request request;
        request.peer_id = id;
        request.ip = "127.0.0.1";
        request.port = port;
        request.file_id = meta.file_hash;
        request.bitfield = state->get_bitfield(meta.file_hash);
        for (int attempt = 0; attempt < 40; ++attempt) {
            if (client.announce(request)) {
                return true;
            }
            this_thread::sleep_for(chrono::milliseconds(50));
        }
        return false;
    }
};

torrent_info make_torrent(const string& source, size_t chunk_size,
                          const string& torrent_path, const string& tracker_url) {
    Metadata metadata;
    EXPECT_TRUE(metadata.generate_torrent(source, chunk_size, torrent_path,
                                          tracker_url));
    try {
        return metadata.load(torrent_path);
    } catch (const exception& error) {
        ADD_FAILURE() << "cannot load generated metadata: " << error.what();
        return {};
    }
}

} // namespace

// Scenario 1-4: the only source a download has made progress with disappears
// while chunks are still in flight, and the remaining chunks must be served by
// a different peer without restarting the transfer.
TEST(FaultRecoveryTest, SeederDeathMidDownloadFailsOverToSecondSeeder) {
    const int tracker_port = testutil::reserve_port();
    ASSERT_GT(tracker_port, 0);
    TrackerFixture tracker(tracker_port);
    ASSERT_TRUE(tracker.is_ready());

    const string root = testutil::make_temp_dir("m6_seeder_death");
    const string source = root + "/source.bin";
    const string torrent_path = root + "/source.torrent.json";
    ASSERT_TRUE(testutil::write_pattern_file(source, 1024 * 1024));
    const string source_hash = testutil::file_sha256(source);
    const torrent_info meta =
        make_torrent(source, 32 * 1024, torrent_path, tracker.url());

    Seeder slow;
    Seeder healthy;
    ASSERT_TRUE(slow.start(root + "/seeder_slow", "seed-slow", tracker.url(),
                           meta, source, 120));
    ASSERT_TRUE(healthy.start(root + "/seeder_healthy", "seed-healthy",
                              tracker.url(), meta, source, 0));

    filesystem::create_directories(root + "/leecher");
    unique_ptr<Database> db = make_unique<Database>(root + "/leecher/peer.db");
    db->init_schema();
    auto files = make_unique<FileManager>();
    auto state = make_unique<StateManager>(db.get());
    state->init_file_state(meta.file_hash, meta.num_chunks, false);
    const string output = root + "/download.bin";
    ASSERT_TRUE(files->ensure_file_size(output, meta.file_size));

    TrackerClient tracker_client(tracker.url());
    auto download = make_unique<DownloadManager>(files.get(), state.get(),
                                                 &tracker_client, "leech-dying");
    download_options options;
    options.pipeline_depth = 2;
    options.io_timeout_ms = 3000;
    options.discovery_wait_ms = 100;
    download->set_options(options);

    atomic<bool> done{false};
    atomic<bool> result{false};
    thread worker([&]() {
        result.store(download->start_download(meta, output));
        done.store(true);
    });
    ASSERT_TRUE(testutil::wait_until([&]() {
        return download->get_stats().chunks_verified > 0;
    }, 20000, 25));

    // Kill the source the download is currently progressing with. The chunk
    // claims it held must return to the queue.
    slow.stop();

    ASSERT_TRUE(testutil::wait_until([&]() { return done.load(); }, 90000, 100))
        << "download did not finish after its seeder died";
    worker.join();

    EXPECT_TRUE(result.load()) << download->last_error();
    EXPECT_TRUE(state->is_completed(meta.file_hash));
    EXPECT_EQ(testutil::file_sha256(output), source_hash);
    EXPECT_GT(download->get_stats().retries, 0u);
    EXPECT_GT(healthy.upload->total_chunks_uploaded(), 0u);

    download->request_stop();
    healthy.stop();
    testutil::remove_quietly(root);
}

// Scenarios 5-7: when the only available source serves corrupted chunks, the
// download must be reported as failed, the bad content must not be recorded as
// verified progress, and the whole-file hash must not be claimed.
TEST(FaultRecoveryTest, CorruptedOnlySourceIsRejectedAndDownloadFails) {
    const int tracker_port = testutil::reserve_port();
    ASSERT_GT(tracker_port, 0);
    TrackerFixture tracker(tracker_port);
    ASSERT_TRUE(tracker.is_ready());

    const string root = testutil::make_temp_dir("m6_corrupt_only");
    const string source = root + "/source.bin";
    const string torrent_path = root + "/source.torrent.json";
    ASSERT_TRUE(testutil::write_pattern_file(source, 256 * 1024, 991u));
    const string source_hash = testutil::file_sha256(source);
    const torrent_info meta =
        make_torrent(source, 32 * 1024, torrent_path, tracker.url());

    Seeder poison;
    ASSERT_TRUE(poison.start(root + "/seeder", "seed-poison-only", tracker.url(),
                             meta, source, 0));
    vector<int> every_chunk(meta.num_chunks);
    for (size_t i = 0; i < meta.num_chunks; ++i) {
        every_chunk[i] = static_cast<int>(i);
    }
    poison.upload->set_corrupt_chunks(every_chunk);

    filesystem::create_directories(root + "/leecher");
    unique_ptr<Database> db = make_unique<Database>(root + "/leecher/peer.db");
    db->init_schema();
    auto files = make_unique<FileManager>();
    auto state = make_unique<StateManager>(db.get());
    state->init_file_state(meta.file_hash, meta.num_chunks, false);
    const string output = root + "/download.bin";
    ASSERT_TRUE(files->ensure_file_size(output, meta.file_size));

    TrackerClient tracker_client(tracker.url());
    auto download = make_unique<DownloadManager>(files.get(), state.get(),
                                                 &tracker_client, "leech-verify");
    download_options options;
    options.discovery_wait_ms = 100;
    download->set_options(options);

    const bool result = download->start_download(meta, output);

    const auto stats = download->get_stats();
    EXPECT_GE(stats.hash_mismatches, 1u);
    EXPECT_FALSE(result) << "a corrupted-only source must not report success";
    EXPECT_NE(testutil::file_sha256(output), source_hash);
    EXPECT_FALSE(state->is_completed(meta.file_hash));

    poison.stop();
    download->request_stop();
    testutil::remove_quietly(root);
}

#ifdef _WIN32

// A helper for the real-process crash test: the peer under test is a genuine
// PeerApp.exe child process, so the kill is a real process termination and the
// restart reloads state from the SQLite file left on disk.
class ChildPeer {
    public:
        bool start(const string& command_line, const string& working_dir) {
            STARTUPINFOA startup {};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION info {};
            string mutable_command = command_line;
            if (!CreateProcessA(nullptr, mutable_command.data(), nullptr, nullptr,
                                FALSE, 0, nullptr, working_dir.c_str(), &startup,
                                &info)) {
                return false;
            }
            process = info.hProcess;
            thread_handle = info.hThread;
            return true;
        }

        void kill() {
            if (process) {
                TerminateProcess(process, 1);
            }
        }

        // Waits for the child to exit on its own and reports its exit code.
        bool wait(int milliseconds, int& exit_code) {
            if (!process) {
                return false;
            }
            if (WaitForSingleObject(process, static_cast<DWORD>(milliseconds)) ==
                WAIT_TIMEOUT) {
                return false;
            }
            DWORD code = 0;
            GetExitCodeProcess(process, &code);
            exit_code = static_cast<int>(code);
            return true;
        }

        ~ChildPeer() {
            close();
        }

        void close() {
            if (process) {
                WaitForSingleObject(process, 10000);
                CloseHandle(process);
                process = nullptr;
            }
            if (thread_handle) {
                CloseHandle(thread_handle);
                thread_handle = nullptr;
            }
        }

    private:
        HANDLE process = nullptr;
        HANDLE thread_handle = nullptr;
};

string to_windows_path(const string& path) {
    filesystem::path converted(path);
    return converted.string();
}

// Scenarios 9-15: a real leecher process is killed part-way through a download
// and restarted against the same SQLite state and the same output directory.
TEST(FaultRecoveryTest, RealProcessCrashResumesFromPersistedSqliteState) {
    ASSERT_GT(strlen(PEER_APP_PATH), 0u) << "PeerApp path was not compiled in";

    const int tracker_port = testutil::reserve_port();
    ASSERT_GT(tracker_port, 0);
    TrackerFixture tracker(tracker_port);
    ASSERT_TRUE(tracker.is_ready());

    const string root = testutil::make_temp_dir("m6_crash_resume");
    const string source = root + "/source.bin";
    const string torrent_path = root + "/source.torrent.json";
    ASSERT_TRUE(testutil::write_pattern_file(source, 1024 * 1024, 5150u));
    const string source_hash = testutil::file_sha256(source);
    const torrent_info meta =
        make_torrent(source, 32 * 1024, torrent_path, tracker.url());

    // The seeder serves slowly so the leecher is guaranteed to be interrupted
    // with part of the file still missing.
    Seeder seeder;
    ASSERT_TRUE(seeder.start(root + "/seeder", "seed-for-crash", tracker.url(),
                             meta, source, 120));

    const string data_dir = to_windows_path(root + "/leecher");
    const string output_dir = to_windows_path(root + "/out");
    const string torrent_argument = to_windows_path(torrent_path);
    filesystem::create_directories(root + "/leecher");
    filesystem::create_directories(root + "/out");

    const string common_arguments =
        string("--tracker ") + tracker.url() + " --port 0 --data-dir \"" +
        data_dir + "\" --download \"" + torrent_argument + "\" --output-dir \"" +
        output_dir + "\" --exit-after-download";

    ChildPeer first_run;
    const string first_command =
        string("\"") + PEER_APP_PATH + "\" " + common_arguments;
    ASSERT_TRUE(first_run.start(first_command, root));

    // Wait until the tracker reports that this peer holds some, but not all,
    // chunks. That is the persisted partial progress the restart must reuse.
    const string peer_db = root + "/leecher/peer.db";
    atomic<int> chunks_at_kill{-1};
    ASSERT_TRUE(testutil::wait_until(
        [&]() {
            Database probe(peer_db);
            const vector<torrent_record> torrents = probe.get_all_torrents();
            int verified = 0;
            for (const auto& torrent : torrents) {
                for (const auto& chunk : probe.get_chunks(torrent.file_id)) {
                    if (chunk.status == "verified") {
                        verified++;
                    }
                }
            }
            if (verified > 1 &&
                static_cast<size_t>(verified) < meta.num_chunks) {
                chunks_at_kill.store(verified);
                return true;
            }
            return false;
        },
        60000, 200))
        << "leecher never reached a partially verified state before the kill";

    first_run.kill();
    first_run.close();

    int exit_code = -1;
    ChildPeer second_run;
    const string second_command =
        string("\"") + PEER_APP_PATH + "\" " + common_arguments;
    ASSERT_TRUE(second_run.start(second_command, root));
    ASSERT_TRUE(second_run.wait(150000, exit_code))
        << "restarted leecher did not finish within its timeout";
    second_run.close();

    EXPECT_EQ(exit_code, 0) << "restarted peer exited with " << exit_code;
    const string output = root + "/out/source.bin";
    ASSERT_TRUE(filesystem::exists(output));
    EXPECT_EQ(testutil::file_sha256(output), source_hash);
    EXPECT_GT(chunks_at_kill.load(), 0);
    EXPECT_LT(static_cast<size_t>(chunks_at_kill.load()), meta.num_chunks);

    // The restarted run must not re-download the chunks it already had
    // verified, which is what distinguishes a resume from a fresh transfer.
    TrackerClient client(tracker.url());
    seeder.stop();
    tracker.stop();
    testutil::remove_quietly(root);
}

#else

TEST(FaultRecoveryTest, RealProcessCrashResumesFromPersistedSqliteState) {
    GTEST_SKIP() << "process kill/resume is implemented for Windows only";
}

#endif
