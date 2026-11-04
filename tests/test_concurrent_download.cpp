#include <gtest/gtest.h>

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

#include "database.hpp"
#include "download_manager.hpp"
#include "file_manager.hpp"
#include "logger.hpp"
#include "metadata.hpp"
#include "peer_node.hpp"
#include "state_manager.hpp"
#include "test_helpers.hpp"
#include "tracker_client.hpp"
#include "tracker_server.hpp"
#include "upload_server.hpp"

using namespace std;

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

struct PeerCommon {
    unique_ptr<Database> db;
    unique_ptr<FileManager> file_manager;
    unique_ptr<StateManager> state_manager;
    unique_ptr<UploadServer> upload;
    string peer_id;
    string directory;
    int port = 0;
    torrent_info meta;
    mutable mutex announce_mtx;
    chrono::steady_clock::time_point last_announce{};

    ~PeerCommon() { stop_upload(); }

    void stop_upload() {
        if (upload) {
            upload->stop();
        }
    }

    bool announce(const string& tracker_url, bool force = false) {
        {
            lock_guard<mutex> lock(announce_mtx);
            const auto now = chrono::steady_clock::now();
            if (!force && now - last_announce < chrono::milliseconds(100)) {
                return true;
            }
            last_announce = now;
        }
        TrackerClient client(tracker_url);
        announce_request request;
        request.peer_id = peer_id;
        request.ip = "127.0.0.1";
        request.port = port;
        request.file_id = meta.file_hash;
        request.bitfield = state_manager->get_bitfield(meta.file_hash);
        return client.announce(request);
    }

    bool announce_with_retry(const string& tracker_url) {
        for (int attempt = 0; attempt < 40; ++attempt) {
            if (announce(tracker_url, true)) {
                return true;
            }
            this_thread::sleep_for(chrono::milliseconds(50));
        }
        return false;
    }
};

struct SeederNode : PeerCommon {
    bool start(const string& dir, const string& id, const string& tracker_url,
               const torrent_info& torrent, const string& file_path,
               const vector<bool>& chunks, int max_uploads = -1) {
        peer_id = id;
        directory = dir;
        meta = torrent;
        filesystem::create_directories(directory);

        db = make_unique<Database>(directory + "/peer.db");
        db->init_schema();
        file_manager = make_unique<FileManager>();
        state_manager = make_unique<StateManager>(db.get());
        state_manager->init_file_state(meta.file_hash, meta.num_chunks, false);
        for (size_t i = 0; i < chunks.size(); ++i) {
            if (chunks[i]) {
                state_manager->mark_chunk_complete(meta.file_hash,
                                                   static_cast<int>(i));
            }
        }

        upload = make_unique<UploadServer>(0, file_manager.get(),
                                           state_manager.get(), peer_id);
        if (max_uploads >= 0) {
            upload_options upload_settings;
            upload_settings.max_active_uploads = max_uploads;
            upload->set_upload_options(upload_settings);
        }
        if (!upload->start()) {
            return false;
        }
        port = upload->get_port();
        upload->register_file(meta.file_hash, file_path, meta.chunk_size,
                              meta.num_chunks);
        return announce_with_retry(tracker_url);
    }
};

struct LeecherNode : PeerCommon {
    unique_ptr<DownloadManager> download;
    unique_ptr<TrackerClient> tracker_client;
    string output_path;

    bool start(const string& dir, const string& id, const string& tracker_url,
               const torrent_info& torrent, const string& output_file,
               int max_downloads = 5, int pipeline_depth = 3) {
        peer_id = id;
        directory = dir;
        meta = torrent;
        output_path = output_file;
        filesystem::create_directories(directory);

        db = make_unique<Database>(directory + "/peer.db");
        db->init_schema();
        file_manager = make_unique<FileManager>();
        state_manager = make_unique<StateManager>(db.get());
        state_manager->init_file_state(meta.file_hash, meta.num_chunks, false);
        if (!file_manager->ensure_file_size(output_path, meta.file_size)) {
            return false;
        }

        upload = make_unique<UploadServer>(0, file_manager.get(),
                                           state_manager.get(), peer_id);
        if (!upload->start()) {
            return false;
        }
        port = upload->get_port();
        upload->register_file(meta.file_hash, output_path, meta.chunk_size,
                              meta.num_chunks);

        tracker_client = make_unique<TrackerClient>(tracker_url);
        download = make_unique<DownloadManager>(file_manager.get(),
                                                state_manager.get(),
                                                tracker_client.get(), peer_id);
        download_options options;
        options.max_active_downloads = max_downloads;
        options.pipeline_depth = pipeline_depth;
        options.connect_timeout_ms = 2000;
        options.io_timeout_ms = 4000;
        options.discovery_wait_ms = 100;
        download->set_options(options);

        return announce_with_retry(tracker_url);
    }

    bool run(const string& tracker_url, int timeout_seconds = 90) {
        return run_blocking(*this, tracker_url, timeout_seconds);
    }

    static bool run_blocking(LeecherNode& node, const string& tracker_url,
                             int timeout_seconds = 90) {
        node.download->set_chunk_verified_callback(
            [&node, tracker_url](const string&, int) {
                node.announce(tracker_url, false);
            });

        auto future = std::async(std::launch::async, [&node]() {
            return node.download->start_download(node.meta, node.output_path);
        });

        const auto status = future.wait_for(chrono::seconds(timeout_seconds));
        if (status != std::future_status::ready) {
            node.download->request_stop();
            future.wait();
            return false;
        }
        return future.get();
    }

    void request_stop() {
        if (download) {
            download->request_stop();
        }
    }
};

torrent_info make_torrent(const string& source_path, size_t chunk_size,
                          const string& torrent_path, const string& tracker_url) {
    Metadata metadata;
    EXPECT_TRUE(metadata.generate_torrent(source_path, chunk_size, torrent_path,
                                          tracker_url));
    try {
        return metadata.load(torrent_path);
    } catch (const exception& error) {
        ADD_FAILURE() << "cannot load generated metadata: " << error.what();
        return {};
    }
}

vector<bool> full_bitfield(size_t chunks) {
    return vector<bool>(chunks, true);
}

vector<bool> parity_bitfield(size_t chunks, int parity) {
    vector<bool> bitfield(chunks, false);
    for (size_t i = 0; i < chunks; ++i) {
        bitfield[i] = (static_cast<int>(i) % 2) == parity;
    }
    return bitfield;
}

// Copies the chunks a peer already owns into its destination file and records
// them as verified. The result is the on-disk state of a partially completed
// leecher: it can serve those chunks while it still downloads the rest.
bool make_partial(LeecherNode& node, const string& source,
                  const vector<bool>& owned, const string& tracker_url) {
    for (size_t i = 0; i < owned.size(); ++i) {
        if (!owned[i]) {
            continue;
        }
        const vector<char> data =
            node.file_manager->read_chunk(source, i, node.meta.chunk_size);
        if (data.empty() ||
            !node.file_manager->write_chunk(node.output_path,
                                            node.meta.chunk_size, i, data)) {
            return false;
        }
        node.state_manager->mark_chunk_complete(node.meta.file_hash,
                                                static_cast<int>(i));
    }
    return node.announce(tracker_url, true);
}

bool run_leechers(vector<LeecherNode*> nodes, const string& tracker_url,
                  int timeout_seconds = 120) {
    vector<future<bool>> futures;
    for (auto* node : nodes) {
        futures.push_back(std::async(std::launch::async, [node, tracker_url]() {
            return LeecherNode::run_blocking(*node, tracker_url);
        }));
    }

    bool all_ok = true;
    for (size_t i = 0; i < futures.size(); ++i) {
        if (futures[i].wait_for(chrono::seconds(timeout_seconds)) !=
            std::future_status::ready) {
            all_ok = false;
            for (auto* node : nodes) {
                node->request_stop();
            }
            futures[i].wait();
            continue;
        }
        all_ok = futures[i].get() && all_ok;
    }
    return all_ok;
}

} // namespace

TEST(ConcurrentDownloadTest, OneSeederTwoLeechersConcurrent) {
    const int tracker_port = testutil::reserve_port();
    ASSERT_GT(tracker_port, 0);
    TrackerFixture tracker(tracker_port);
    ASSERT_TRUE(tracker.is_ready());

    const string root = testutil::make_temp_dir("m5_one_seeder_two_leechers");
    const string source = root + "/source.bin";
    const string torrent_path = root + "/source.torrent.json";
    // Issue #11 specifies a 5 MB file split into many small chunks.
    const size_t file_size = 5 * 1024 * 1024;
    const size_t chunk_size = 64 * 1024;
    ASSERT_TRUE(testutil::write_pattern_file(source, file_size));
    const string source_hash = testutil::file_sha256(source);
    const torrent_info meta =
        make_torrent(source, chunk_size, torrent_path, tracker.url());

    SeederNode seeder;
    ASSERT_TRUE(seeder.start(root + "/seeder", "seed-1", tracker.url(), meta,
                             source, full_bitfield(meta.num_chunks)));

    LeecherNode first;
    LeecherNode second;
    ASSERT_TRUE(first.start(root + "/leecher1", "leech-1", tracker.url(), meta,
                            root + "/leecher1.bin", 5, 3));
    ASSERT_TRUE(second.start(root + "/leecher2", "leech-2", tracker.url(), meta,
                             root + "/leecher2.bin", 5, 3));

    ASSERT_TRUE(run_leechers({&first, &second}, tracker.url()));

    EXPECT_EQ(testutil::file_sha256(first.output_path), source_hash);
    EXPECT_EQ(testutil::file_sha256(second.output_path), source_hash);
    EXPECT_TRUE(first.state_manager->is_completed(meta.file_hash));
    EXPECT_TRUE(second.state_manager->is_completed(meta.file_hash));

    const auto stats = first.download->get_stats();
    EXPECT_GE(stats.peak_active_peers, 1);
    EXPECT_GE(stats.chunks_verified, meta.num_chunks);

    seeder.stop_upload();
    first.stop_upload();
    second.stop_upload();
}

TEST(ConcurrentDownloadTest, LeecherServesAnotherLeecher) {
    const int tracker_port = testutil::reserve_port();
    ASSERT_GT(tracker_port, 0);
    TrackerFixture tracker(tracker_port);
    ASSERT_TRUE(tracker.is_ready());

    const string root = testutil::make_temp_dir("m5_leecher_serves");
    const string source = root + "/source.bin";
    const string torrent_path = root + "/source.torrent.json";
    ASSERT_TRUE(testutil::write_pattern_file(source, 1536 * 1024));
    const string source_hash = testutil::file_sha256(source);
    const torrent_info meta =
        make_torrent(source, 64 * 1024, torrent_path, tracker.url());

    SeederNode seeder;
    ASSERT_TRUE(seeder.start(root + "/seeder", "seed-master", tracker.url(),
                             meta, source, full_bitfield(meta.num_chunks)));

    LeecherNode first;
    ASSERT_TRUE(first.start(root + "/leecher1", "leech-source", tracker.url(),
                            meta, root + "/leecher1.bin"));
    ASSERT_TRUE(LeecherNode::run_blocking(first, tracker.url()));
    EXPECT_TRUE(first.state_manager->is_completed(meta.file_hash));

    // The seeder disappears but the first leecher keeps its upload server and
    // is still registered in the tracker, so the second leecher must fetch the
    // complete file from the first leecher.
    seeder.stop_upload();
    EXPECT_TRUE(first.announce(tracker.url(), true));

    LeecherNode second;
    ASSERT_TRUE(second.start(root + "/leecher2", "leech-consumer",
                             tracker.url(), meta, root + "/leecher2.bin"));
    ASSERT_TRUE(LeecherNode::run_blocking(second, tracker.url()));

    EXPECT_EQ(testutil::file_sha256(second.output_path), source_hash);
    EXPECT_TRUE(second.state_manager->is_completed(meta.file_hash));
    EXPECT_GT(first.upload->total_bytes_uploaded(), 0u);

    first.stop_upload();
    second.stop_upload();
}

TEST(ConcurrentDownloadTest, TwoSeedersOneLeecher) {
    const int tracker_port = testutil::reserve_port();
    ASSERT_GT(tracker_port, 0);
    TrackerFixture tracker(tracker_port);
    ASSERT_TRUE(tracker.is_ready());

    const string root = testutil::make_temp_dir("m5_two_seeders");
    const string source = root + "/source.bin";
    const string torrent_path = root + "/source.torrent.json";
    ASSERT_TRUE(testutil::write_pattern_file(source, 1024 * 1024));
    const string source_hash = testutil::file_sha256(source);
    const torrent_info meta =
        make_torrent(source, 32 * 1024, torrent_path, tracker.url());

    SeederNode even;
    SeederNode odd;
    ASSERT_TRUE(even.start(root + "/seeder_even", "seed-even", tracker.url(),
                           meta, source, parity_bitfield(meta.num_chunks, 0)));
    ASSERT_TRUE(odd.start(root + "/seeder_odd", "seed-odd", tracker.url(), meta,
                          source, parity_bitfield(meta.num_chunks, 1)));

    LeecherNode leecher;
    ASSERT_TRUE(leecher.start(root + "/leecher", "leech-multi", tracker.url(),
                              meta, root + "/download.bin", 5, 4));
    ASSERT_TRUE(LeecherNode::run_blocking(leecher, tracker.url()));

    EXPECT_EQ(testutil::file_sha256(leecher.output_path), source_hash);
    EXPECT_TRUE(leecher.state_manager->is_completed(meta.file_hash));
    EXPECT_GT(even.upload->total_bytes_uploaded(), 0u);
    EXPECT_GT(odd.upload->total_bytes_uploaded(), 0u);

    even.stop_upload();
    odd.stop_upload();
    leecher.stop_upload();
}

TEST(ConcurrentDownloadTest, UploadSlotsRespectLimitUnderConnectionSaturation) {
    const int tracker_port = testutil::reserve_port();
    ASSERT_GT(tracker_port, 0);
    TrackerFixture tracker(tracker_port);
    ASSERT_TRUE(tracker.is_ready());

    const string root = testutil::make_temp_dir("m5_upload_limit");
    const string source = root + "/source.bin";
    const string torrent_path = root + "/source.torrent.json";
    ASSERT_TRUE(testutil::write_pattern_file(source, 512 * 1024));
    const string source_hash = testutil::file_sha256(source);
    const torrent_info meta =
        make_torrent(source, 32 * 1024, torrent_path, tracker.url());

    constexpr int upload_limit = 2;
    SeederNode seeder;
    ASSERT_TRUE(seeder.start(root + "/seeder", "seed-limited", tracker.url(),
                             meta, source, full_bitfield(meta.num_chunks),
                             upload_limit));

    // Four leechers compete for two upload slots, so excess connections must
    // wait for a slot rather than being served past the configured cap.
    vector<unique_ptr<LeecherNode>> leechers;
    vector<LeecherNode*> running;
    for (int i = 0; i < 4; ++i) {
        const string tag = to_string(i);
        auto node = make_unique<LeecherNode>();
        ASSERT_TRUE(node->start(root + "/leecher_" + tag, "leech-sat-" + tag,
                                tracker.url(), meta,
                                root + "/leecher_" + tag + ".bin"));
        running.push_back(node.get());
        leechers.push_back(std::move(node));
    }

    ASSERT_TRUE(run_leechers(running, tracker.url(), 180));

    // Four leechers need more throughput than two slots provide, so the pool
    // must run saturated without ever handing out a third slot.
    EXPECT_EQ(seeder.upload->peak_upload_slots(), upload_limit);
    for (auto& node : leechers) {
        EXPECT_EQ(testutil::file_sha256(node->output_path), source_hash);
        EXPECT_TRUE(node->state_manager->is_completed(meta.file_hash));
    }

    for (auto& node : leechers) {
        node->stop_upload();
    }
    seeder.stop_upload();
}

TEST(ConcurrentDownloadTest, PartialPeersServeEachOtherWhileDownloading) {
    const int tracker_port = testutil::reserve_port();
    ASSERT_GT(tracker_port, 0);
    TrackerFixture tracker(tracker_port);
    ASSERT_TRUE(tracker.is_ready());

    const string root = testutil::make_temp_dir("m5_full_duplex");
    const string source = root + "/source.bin";
    const string torrent_path = root + "/source.torrent.json";
    ASSERT_TRUE(testutil::write_pattern_file(source, 1024 * 1024));
    const string source_hash = testutil::file_sha256(source);
    const torrent_info meta =
        make_torrent(source, 32 * 1024, torrent_path, tracker.url());

    // No peer holds the whole file. Each one must upload its own half to the
    // other at the same time as it downloads the half it is missing, so an
    // UploadServer blocked behind its own DownloadManager hangs this test
    // instead of passing it.
    LeecherNode even;
    LeecherNode odd;
    ASSERT_TRUE(even.start(root + "/peer_even", "duplex-even", tracker.url(),
                           meta, root + "/peer_even.bin"));
    ASSERT_TRUE(odd.start(root + "/peer_odd", "duplex-odd", tracker.url(), meta,
                          root + "/peer_odd.bin"));
    ASSERT_TRUE(make_partial(even, source, parity_bitfield(meta.num_chunks, 0),
                             tracker.url()));
    ASSERT_TRUE(make_partial(odd, source, parity_bitfield(meta.num_chunks, 1),
                             tracker.url()));

    ASSERT_TRUE(run_leechers({&even, &odd}, tracker.url()));

    EXPECT_EQ(testutil::file_sha256(even.output_path), source_hash);
    EXPECT_EQ(testutil::file_sha256(odd.output_path), source_hash);
    EXPECT_TRUE(even.state_manager->is_completed(meta.file_hash));
    EXPECT_TRUE(odd.state_manager->is_completed(meta.file_hash));
    EXPECT_GT(even.upload->total_bytes_uploaded(), 0u);
    EXPECT_GT(odd.upload->total_bytes_uploaded(), 0u);

    even.stop_upload();
    odd.stop_upload();
}

// Two complete PeerNode objects, driven through the same API the CLI uses,
// both requesting listening port 0. Their identities must stay distinct: if
// the id were derived only from the requested port the leecher would treat the
// seeder as itself, skip it during discovery, and never transfer a chunk.
TEST(ConcurrentDownloadTest, PeerNodesOnSameRequestedPortExchange) {
    const int tracker_port = testutil::reserve_port();
    ASSERT_GT(tracker_port, 0);
    TrackerFixture tracker(tracker_port);
    ASSERT_TRUE(tracker.is_ready());

    const string root = testutil::make_temp_dir("m5_peer_node");
    const string source = root + "/source.bin";
    ASSERT_TRUE(testutil::write_pattern_file(source, 512 * 1024, 4242u));
    const string source_hash = testutil::file_sha256(source);

    PeerNode seeder(tracker.url(), 0, root + "/seeder");
    ASSERT_TRUE(seeder.start());
    ASSERT_TRUE(seeder.seed_file(source, 32 * 1024));

    PeerNode leecher(tracker.url(), 0, root + "/leecher");
    ASSERT_TRUE(leecher.start());
    EXPECT_NE(leecher.get_peer_id(), seeder.get_peer_id());

    const string torrent_path = source + ".torrent.json";
    auto future = std::async(std::launch::async, [&leecher, &torrent_path, &root]() {
        return leecher.download_file(torrent_path, root + "/out");
    });
    if (future.wait_for(chrono::seconds(90)) != std::future_status::ready) {
        leecher.request_stop_download();
        future.wait();
        FAIL() << "PeerNode download did not finish within 90 seconds";
    }
    ASSERT_TRUE(future.get());

    EXPECT_EQ(testutil::file_sha256(root + "/out/source.bin"), source_hash);
    EXPECT_NE(leecher.get_port(), seeder.get_port());

    leecher.stop();
    seeder.stop();
}

// Issue #9 requires per-chunk SHA-256 with failover, and issue #13 requires that
// corrupted data is rejected. The malicious seeder corrupts every chunk it
// serves, so any chunk the leecher takes from it must be detected, blacklisted
// for that chunk, and re-fetched from the honest seeder.
TEST(ConcurrentDownloadTest, CorruptedChunksAreDetectedRequeuedAndRefetched) {
    const int tracker_port = testutil::reserve_port();
    ASSERT_GT(tracker_port, 0);
    TrackerFixture tracker(tracker_port);
    ASSERT_TRUE(tracker.is_ready());

    const string root = testutil::make_temp_dir("m5_corruption");
    const string source = root + "/source.bin";
    const string torrent_path = root + "/source.torrent.json";
    ASSERT_TRUE(testutil::write_pattern_file(source, 512 * 1024, 777u));
    const string source_hash = testutil::file_sha256(source);
    const torrent_info meta =
        make_torrent(source, 32 * 1024, torrent_path, tracker.url());

    SeederNode poison;
    ASSERT_TRUE(poison.start(root + "/seeder_poison", "seed-poison",
                             tracker.url(), meta, source,
                             full_bitfield(meta.num_chunks)));
    vector<int> every_chunk(meta.num_chunks);
    for (size_t i = 0; i < meta.num_chunks; ++i) {
        every_chunk[i] = static_cast<int>(i);
    }
    poison.upload->set_corrupt_chunks(every_chunk);

    SeederNode honest;
    ASSERT_TRUE(honest.start(root + "/seeder_honest", "seed-honest",
                             tracker.url(), meta, source,
                             full_bitfield(meta.num_chunks)));

    LeecherNode leecher;
    ASSERT_TRUE(leecher.start(root + "/leecher", "leech-verify", tracker.url(),
                              meta, root + "/download.bin", 5, 4));
    ASSERT_TRUE(LeecherNode::run_blocking(leecher, tracker.url()));

    const auto stats = leecher.download->get_stats();
    EXPECT_GE(stats.hash_mismatches, 1u);
    EXPECT_GE(stats.retries, 1u);
    EXPECT_TRUE(leecher.state_manager->is_completed(meta.file_hash));
    EXPECT_EQ(testutil::file_sha256(leecher.output_path), source_hash);

    poison.stop_upload();
    honest.stop_upload();
    leecher.stop_upload();
}

// A peer that registers after the download began must be adopted promptly.
// Without periodic re-discovery the tracker is only re-asked once every session
// has dropped, and an idle worker holds its slot for the whole spin budget, so
// the new source sat unnoticed for roughly two seconds.
TEST(ConcurrentDownloadTest, PeerRegisteringAfterStartIsAdoptedPromptly) {
    const int tracker_port = testutil::reserve_port();
    ASSERT_GT(tracker_port, 0);
    TrackerFixture tracker(tracker_port);
    ASSERT_TRUE(tracker.is_ready());

    const string root = testutil::make_temp_dir("m5_late_peer");
    const string source = root + "/source.bin";
    const string torrent_path = root + "/source.torrent.json";
    ASSERT_TRUE(testutil::write_pattern_file(source, 512 * 1024, 31337u));
    const string source_hash = testutil::file_sha256(source);
    const torrent_info meta =
        make_torrent(source, 32 * 1024, torrent_path, tracker.url());

    vector<bool> first_half(meta.num_chunks, false);
    for (size_t i = 0; i < meta.num_chunks / 2; ++i) {
        first_half[i] = true;
    }

    SeederNode early;
    ASSERT_TRUE(early.start(root + "/seed_early", "seed-early-half",
                            tracker.url(), meta, source, first_half));

    LeecherNode leecher;
    ASSERT_TRUE(leecher.start(root + "/leecher", "leech-late-peer",
                              tracker.url(), meta, root + "/download.bin", 5, 4));

    const auto started = chrono::steady_clock::now();
    auto future = std::async(std::launch::async, [&]() {
        return LeecherNode::run_blocking(leecher, tracker.url());
    });

    // Let the download reach its idle state, then introduce the only peer that
    // holds the remaining half.
    this_thread::sleep_for(chrono::milliseconds(1200));
    SeederNode late;
    ASSERT_TRUE(late.start(root + "/seed_late", "seed-late-full", tracker.url(),
                           meta, source, full_bitfield(meta.num_chunks)));

    const bool ready = future.wait_for(chrono::seconds(30)) == std::future_status::ready;
    const auto elapsed_ms = chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now() - started).count();
    std::cout << "LATE_PEER_PROBE elapsed_ms=" << elapsed_ms << "\n";

    ASSERT_TRUE(ready) << "the late peer was never adopted";
    EXPECT_TRUE(future.get());
    EXPECT_EQ(testutil::file_sha256(leecher.output_path), source_hash);
    EXPECT_TRUE(leecher.state_manager->is_completed(meta.file_hash));
    EXPECT_LT(elapsed_ms, 2200)
        << "adopting a peer that appeared mid-download took " << elapsed_ms
        << " ms, so the scheduler only noticed it after idle sessions expired";

    late.stop_upload();
    early.stop_upload();
    leecher.stop_upload();
}

TEST(ConcurrentDownloadTest, RepeatedSwarmStressWithoutDeadlock) {
    const int tracker_port = testutil::reserve_port();
    ASSERT_GT(tracker_port, 0);
    TrackerFixture tracker(tracker_port);
    ASSERT_TRUE(tracker.is_ready());

    const string root = testutil::make_temp_dir("m5_stress");
    for (int iteration = 0; iteration < 3; ++iteration) {
        const string tag = to_string(iteration);
        const string source = root + "/source_" + tag + ".bin";
        const string torrent_path = root + "/source_" + tag + ".torrent.json";
        ASSERT_TRUE(testutil::write_pattern_file(source, 512 * 1024,
                                                 17u + iteration * 31u));
        const string source_hash = testutil::file_sha256(source);
        const torrent_info meta =
            make_torrent(source, 32 * 1024, torrent_path, tracker.url());

        SeederNode first;
        SeederNode second;
        ASSERT_TRUE(first.start(root + "/seed_a_" + tag, "stress-seed-a-" + tag,
                                tracker.url(), meta, source,
                                full_bitfield(meta.num_chunks)));
        ASSERT_TRUE(second.start(root + "/seed_b_" + tag, "stress-seed-b-" + tag,
                                 tracker.url(), meta, source,
                                 full_bitfield(meta.num_chunks)));

        LeecherNode leecher;
        ASSERT_TRUE(leecher.start(root + "/leech_" + tag, "stress-leech-" + tag,
                                  tracker.url(), meta,
                                  root + "/download_" + tag + ".bin",
                                  1 + iteration, 1 + iteration));
        ASSERT_TRUE(LeecherNode::run_blocking(leecher, tracker.url(), 120));
        EXPECT_EQ(testutil::file_sha256(leecher.output_path), source_hash)
            << "iteration " << iteration;

        first.stop_upload();
        second.stop_upload();
        leecher.stop_upload();
    }
}
