#include "peer_node.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>

#include "logger.hpp"
#include "sha256.hpp"

using namespace std;
using namespace std::chrono;

namespace
{

    string resolve_tracker_url(const string &configured_url)
    {
        if (!configured_url.empty())
        {
            return configured_url;
        }

        const char *tracker_url_env = std::getenv("TRACKER_URL");
        return tracker_url_env && *tracker_url_env
                   ? tracker_url_env
                   : "http://localhost:8080";
    }

} // namespace

PeerNode::PeerNode(const string &tracker_url, int port, const string &data_dir)
    : tracker_url(resolve_tracker_url(tracker_url)), port(port), data_dir(data_dir)
{
    filesystem::create_directories(data_dir);
    // Identity has to be unique per process. Several peers routinely request
    // the same listening port (0 means "bind any free port") and the
    // DownloadManager skips every peer whose id matches its own, so an id
    // derived only from the requested port silently hides the rest of the
    // swarm from the peer that reuses it.
    uniform_int_distribution<uint64_t> distribution(0,
                                                    numeric_limits<uint64_t>::max());
    const uint64_t seed = static_cast<uint64_t>(random_device{}()) ^
                          static_cast<uint64_t>(
                              steady_clock::now().time_since_epoch().count()) ^
                          static_cast<uint64_t>(::getpid());
    default_random_engine engine(seed);
    ostringstream identity;
    identity << "peer-" << port << '-' << setfill('0') << setw(16) << hex
             << distribution(engine);
    peer_id = identity.str();

    const string db_path = data_dir + "/peer.db";
    db = make_unique<Database>(db_path);
    db->init_schema();

    file_manager = make_unique<FileManager>();
    state_manager = make_unique<StateManager>(db.get());
    tracker_client = make_unique<TrackerClient>(tracker_url);
    upload_server = make_unique<UploadServer>(port, file_manager.get(),
                                              state_manager.get(), peer_id);
    download_manager = make_unique<DownloadManager>(
        file_manager.get(), state_manager.get(), tracker_client.get(), peer_id);
    last_announce = steady_clock::now() - seconds(10);
}

PeerNode::~PeerNode()
{
    stop();
}

void PeerNode::set_options(const peer_node_options &new_options)
{
    options = new_options;

    download_options download;
    download.max_active_downloads = std::max(1, new_options.max_active_downloads);
    download.pipeline_depth = std::max(1, new_options.pipeline_depth);
    download.io_timeout_ms = std::max(500, new_options.io_timeout_ms);
    download.connect_timeout_ms = std::max(500, new_options.io_timeout_ms);
    download_manager->set_options(download);

    upload_options upload;
    upload.max_active_uploads = std::max(1, new_options.max_active_uploads);
    upload.io_timeout_ms = std::max(500, new_options.io_timeout_ms);
    upload.chunk_delay_ms = std::max(0, new_options.chunk_delay_ms);
    upload_server->set_upload_options(upload);

    if (!new_options.corrupt_chunks.empty())
    {
        upload_server->set_corrupt_chunks(new_options.corrupt_chunks);
    }
}

peer_node_options PeerNode::get_options() const
{
    return options;
}

bool PeerNode::start()
{
    Logger log;
    if (is_running.exchange(true))
    {
        return true;
    }

    log.info("Starting PeerNode [" + peer_id + "] on port " + to_string(port));

    if (!upload_server->start())
    {
        log.err("Failed to start UploadServer on port " + to_string(port));
        is_running.store(false);
        return false;
    }
    port = upload_server->get_port();

    start_heartbeat();
    tracker_online.store(tracker_client->health_check());

    log.success("PeerNode is up and running on port " + to_string(port));
    return true;
}

void PeerNode::stop()
{
    Logger log;
    if (!is_running.exchange(false))
    {
        return;
    }

    log.info("Stopping PeerNode [" + peer_id + "]...");

    if (download_manager)
    {
        download_manager->request_stop();
    }
    if (download_thread.joinable())
    {
        download_thread.join();
    }

    if (heartbeat_thread.joinable())
    {
        heartbeat_thread.join();
    }
    if (upload_server)
    {
        upload_server->stop();
    }

    log.success("PeerNode stopped cleanly");
}

void PeerNode::start_heartbeat()
{
    heartbeat_thread = thread([this]()
                              {
        while (is_running.load()) {
            vector<string> files_to_announce;
            {
                lock_guard<mutex> lock(active_files_mtx);
                files_to_announce.assign(active_files.begin(), active_files.end());
            }
            for (const auto& file_id : files_to_announce) {
                announce_file(file_id, true);
            }

            for (int i = 0; i < 30 && is_running.load(); ++i) {
                this_thread::sleep_for(milliseconds(500));
            }
        } });
}

bool PeerNode::announce_file(const string &file_id, bool force)
{
    if (file_id.empty())
    {
        return false;
    }

    {
        lock_guard<mutex> lock(announce_mtx);
        const auto now = steady_clock::now();
        if (!force && now - last_announce < milliseconds(750))
        {
            return true;
        }
        last_announce = now;
    }

    announce_request request;
    request.peer_id = peer_id;
    const char *peer_host_env = std::getenv("PEER_HOST");
    request.ip = peer_host_env && *peer_host_env ? peer_host_env : "127.0.0.1";
    request.port = port;
    request.file_id = file_id;
    request.bitfield = state_manager->get_bitfield(file_id);

    const bool ok = tracker_client->announce(request);
    tracker_online.store(ok);
    return ok;
}

void PeerNode::remember_torrent(const string &file_id, const string &file_name,
                                const string &output_path, const string &mode,
                                size_t file_size, size_t chunk_size,
                                size_t num_chunks)
{
    managed_torrent_status status;
    status.file_id = file_id;
    status.file_name = file_name;
    status.output_path = output_path;
    status.mode = mode;
    status.file_size = file_size;
    status.chunk_size = chunk_size;
    status.num_chunks = num_chunks;

    lock_guard<mutex> lock(torrents_mtx);
    torrents[file_id] = status;
}

void PeerNode::update_torrent_mode(const string &file_id, const string &mode)
{
    lock_guard<mutex> lock(torrents_mtx);
    auto found = torrents.find(file_id);
    if (found != torrents.end())
    {
        found->second.mode = mode;
    }
}

bool PeerNode::seed_file(const string &file_path, size_t chunk_size)
{
    Logger log;
    if (!filesystem::exists(file_path))
    {
        log.err("Seed file does not exist: " + file_path);
        return false;
    }

    const string torrent_path = file_path + ".torrent.json";
    Metadata meta_tool;
    if (!filesystem::exists(torrent_path))
    {
        log.info("Generating metadata for file " + file_path);
        if (!meta_tool.generate_torrent(file_path, chunk_size, torrent_path,
                                        tracker_url))
        {
            log.err("Failed to generate metadata for " + file_path);
            return false;
        }
    }

    torrent_info meta;
    try
    {
        meta = meta_tool.load(torrent_path);
    }
    catch (const exception &e)
    {
        log.err("Cannot load torrent metadata: " + string(e.what()));
        return false;
    }

    const string file_id = meta.file_hash;
    torrent_record record;
    record.file_id = file_id;
    record.file_name = meta.file_name;
    record.file_size = meta.file_size;
    record.chunk_size = meta.chunk_size;
    record.num_chunks = meta.num_chunks;
    record.file_hash = meta.file_hash;
    record.status = "seeding";
    record.metadata_path = torrent_path;
    record.output_path = file_path;
    record.uploaded = 0;
    record.downloaded = meta.file_size;
    record.added_at = "";
    db->upsert_torrent(record);

    upload_server->register_file(file_id, file_path, meta.chunk_size,
                                 meta.num_chunks);

    state_manager->init_file_state(file_id, meta.num_chunks, true);
    state_manager->mark_all_complete(file_id);

    {
        lock_guard<mutex> lock(active_files_mtx);
        active_files.insert(file_id);
    }
    remember_torrent(file_id, meta.file_name, file_path, "seeding",
                     meta.file_size, meta.chunk_size, meta.num_chunks);

    announce_file(file_id, true);
    log.success("Seeding file: " + meta.file_name + " (File ID: " + file_id + ")");
    return true;
}

bool PeerNode::download_file(const string &torrent_path, const string &output_dir)
{
    Logger log;
    if (download_active.exchange(true))
    {
        log.warnning("A download is already active");
        return false;
    }
    download_success.store(false);

    auto finish_download = [this](bool result)
    {
        download_active.store(false);
        download_success.store(result);
        return result;
    };

    if (!filesystem::exists(torrent_path))
    {
        log.err("Torrent file does not exist: " + torrent_path);
        return finish_download(false);
    }

    torrent_info meta;
    try
    {
        Metadata meta_tool;
        meta = meta_tool.load(torrent_path);
    }
    catch (const exception &e)
    {
        log.err("Cannot load torrent metadata: " + string(e.what()));
        return finish_download(false);
    }

    const string file_id = meta.file_hash;
    const string dest_dir = output_dir.empty() ? data_dir : output_dir;
    filesystem::create_directories(dest_dir);
    const string out_file_path = dest_dir + "/" + meta.file_name;

    torrent_record record;
    record.file_id = file_id;
    record.file_name = meta.file_name;
    record.file_size = meta.file_size;
    record.chunk_size = meta.chunk_size;
    record.num_chunks = meta.num_chunks;
    record.file_hash = meta.file_hash;
    record.status = "downloading";
    record.metadata_path = torrent_path;
    record.output_path = out_file_path;
    record.uploaded = 0;
    record.downloaded = 0;
    record.added_at = "";
    db->upsert_torrent(record);

    state_manager->init_file_state(file_id, meta.num_chunks, false);

    // Register before downloading so verified chunks can be served to other
    // peers while this peer is still a leecher (full-duplex operation).
    upload_server->register_file(file_id, out_file_path, meta.chunk_size,
                                 meta.num_chunks);

    {
        lock_guard<mutex> lock(active_files_mtx);
        active_files.insert(file_id);
    }
    remember_torrent(file_id, meta.file_name, out_file_path, "downloading",
                     meta.file_size, meta.chunk_size, meta.num_chunks);

    announce_file(file_id, true);

    download_manager->set_chunk_verified_callback(
        [this](const string &verified_file_id, int)
        {
            // Re-announce the new bitfield, throttled inside announce_file().
            announce_file(verified_file_id, false);
        });

    log.info("Starting download of " + meta.file_name + " to " + out_file_path);
    const bool success = download_manager->start_download(meta, out_file_path);

    download_manager->set_chunk_verified_callback(nullptr);

    if (success)
    {
        record.status = "completed";
        record.downloaded = meta.file_size;
        db->upsert_torrent(record);
        update_torrent_mode(file_id, "completed");
        announce_file(file_id, true);
        log.success("Successfully downloaded file: " + meta.file_name);
    }
    else
    {
        record.status = "incomplete";
        record.downloaded =
            state_manager->get_completed_chunks_count(file_id) * meta.chunk_size;
        db->upsert_torrent(record);
        update_torrent_mode(file_id, "failed");
        log.warnning("Download ended without a complete file: " + meta.file_name);
    }

    return finish_download(success);
}

bool PeerNode::start_download_async(const string &torrent_path,
                                    const string &output_dir)
{
    if (download_active.load())
    {
        return false;
    }
    if (download_thread.joinable())
    {
        download_thread.join();
    }
    download_thread =
        thread([this, torrent_path, output_dir]()
               { download_file(torrent_path, output_dir); });
    return true;
}

void PeerNode::wait_for_download()
{
    if (download_thread.joinable())
    {
        download_thread.join();
    }
}

void PeerNode::pause_download()
{
    download_manager->pause_download("");
}

void PeerNode::resume_download(bool resume)
{
    if (resume)
    {
        download_manager->resume_download("");
    }
    else
    {
        download_manager->pause_download("");
    }
}

void PeerNode::request_stop_download()
{
    download_manager->request_stop();
}

peer_node_status PeerNode::get_status() const
{
    peer_node_status status;
    status.peer_id = peer_id;
    status.port = port;
    status.running = is_running.load();
    status.tracker_online = tracker_online.load();
    status.download_active = download_active.load();
    status.download = download_manager->get_stats();
    status.download_peers = download_manager->get_peer_status();
    status.upload_peers = upload_server->get_peer_status();
    status.uploaded_bytes = upload_server->total_bytes_uploaded();
    status.uploaded_chunks = upload_server->total_chunks_uploaded();
    status.active_upload_slots = upload_server->active_upload_slots();
    status.peak_upload_slots = upload_server->peak_upload_slots();

    {
        lock_guard<mutex> lock(torrents_mtx);
        status.torrents.reserve(torrents.size());
        for (const auto &entry : torrents)
        {
            managed_torrent_status torrent = entry.second;
            torrent.completed_chunks =
                state_manager->get_completed_chunks_count(torrent.file_id);
            if (torrent.num_chunks > 0)
            {
                torrent.progress =
                    100.0 * static_cast<double>(torrent.completed_chunks) /
                    static_cast<double>(torrent.num_chunks);
            }
            if (torrent.mode == "downloading" &&
                torrent.completed_chunks == torrent.num_chunks &&
                torrent.num_chunks > 0)
            {
                torrent.mode = "completed";
            }
            status.torrents.push_back(torrent);
        }
    }
    return status;
}
