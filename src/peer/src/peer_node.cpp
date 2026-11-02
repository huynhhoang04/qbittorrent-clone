#include "peer_node.hpp"
#include "logger.hpp"
#include "sha256.hpp"
#include <filesystem>
#include <chrono>

using namespace std;

PeerNode::PeerNode(const string& tracker_url, int port, const string& data_dir)
    : tracker_url(tracker_url), port(port), data_dir(data_dir), is_running(false) {
    filesystem::create_directories(data_dir);
    peer_id = "peer-" + to_string(port);

    string db_path = data_dir + "/peer.db";
    db = make_unique<Database>(db_path);
    db->init_schema();

    file_manager = make_unique<FileManager>();
    state_manager = make_unique<StateManager>(db.get());
    tracker_client = make_unique<TrackerClient>(tracker_url);
    upload_server = make_unique<UploadServer>(port, file_manager.get(), state_manager.get(), peer_id);
    download_manager = make_unique<DownloadManager>(file_manager.get(), state_manager.get(), tracker_client.get(), peer_id);
}

PeerNode::~PeerNode() {
    stop();
}

bool PeerNode::start() {
    Logger log;
    if (is_running.exchange(true)) return true;

    log.info("Starting PeerNode [" + peer_id + "] on port " + to_string(port));

    // Khởi động UploadServer
    if (!upload_server->start()) {
        log.err("Failed to start UploadServer on port " + to_string(port));
        is_running = false;
        return false;
    }

    // Khởi động heartbeat thread announce lên Tracker mỗi 15 giây
    start_heartbeat();

    log.success("PeerNode is up and running");
    return true;
}

void PeerNode::stop() {
    Logger log;
    if (!is_running.exchange(false)) return;

    log.info("Stopping PeerNode [" + peer_id + "]...");

    if (upload_server) {
        upload_server->stop();
    }

    if (heartbeat_thread.joinable()) {
        heartbeat_thread.join();
    }

    log.success("PeerNode stopped cleanly");
}

void PeerNode::start_heartbeat() {
    heartbeat_thread = thread([this]() {
        Logger log;
        while (is_running.load()) {
            vector<string> files_to_announce;
            {
                lock_guard<mutex> lock(active_files_mtx);
                files_to_announce.assign(active_files.begin(), active_files.end());
            }

            for (const auto& fid : files_to_announce) {
                announce_request req;
                req.peer_id = peer_id;
                req.ip = "127.0.0.1";
                req.port = port;
                req.file_id = fid;
                req.bitfield = state_manager ? state_manager->get_bitfield(fid) : vector<bool>();

                if (tracker_client) {
                    tracker_client->announce(req);
                }
            }

            // Ngủ 15 giây
            for (int i = 0; i < 15 && is_running.load(); ++i) {
                this_thread::sleep_for(chrono::seconds(1));
            }
        }
    });
}

bool PeerNode::seed_file(const string& file_path, size_t chunk_size) {
    Logger log;
    if (!filesystem::exists(file_path)) {
        log.err("Seed file does not exist: " + file_path);
        return false;
    }

    string torrent_path = file_path + ".torrent.json";
    Metadata meta_tool;
    if (!filesystem::exists(torrent_path)) {
        log.info("Generating metadata for file " + file_path);
        if (!meta_tool.generate_torrent(file_path, chunk_size, torrent_path, tracker_url)) {
            log.err("Failed to generate metadata for " + file_path);
            return false;
        }
    }

    torrent_info meta = meta_tool.load(torrent_path);
    string file_id = meta.file_hash;

    // Lưu vào database
    torrent_record tr;
    tr.file_id = file_id;
    tr.file_name = meta.file_name;
    tr.file_size = meta.file_size;
    tr.chunk_size = meta.chunk_size;
    tr.num_chunks = meta.num_chunks;
    tr.file_hash = meta.file_hash;
    tr.status = "seeding";
    tr.metadata_path = torrent_path;
    tr.output_path = file_path;
    tr.uploaded = 0;
    tr.downloaded = meta.file_size;
    tr.added_at = "";
    db->upsert_torrent(tr);

    // Đăng ký file với UploadServer
    upload_server->register_file(file_id, file_path, meta.chunk_size);

    // Khởi tạo trạng thái seeder 100% trong StateManager
    state_manager->init_file_state(file_id, meta.num_chunks, true);

    {
        lock_guard<mutex> lock(active_files_mtx);
        active_files.insert(file_id);
    }

    // Announce ngay lập tức lên Tracker
    announce_request req;
    req.peer_id = peer_id;
    req.ip = "127.0.0.1";
    req.port = port;
    req.file_id = file_id;
    req.bitfield = state_manager->get_bitfield(file_id);
    tracker_client->announce(req);

    log.success("Seeding file: " + meta.file_name + " (File ID: " + file_id + ")");
    return true;
}

bool PeerNode::download_file(const string& torrent_path, const string& output_dir) {
    Logger log;
    if (!filesystem::exists(torrent_path)) {
        log.err("Torrent file does not exist: " + torrent_path);
        return false;
    }

    Metadata meta_tool;
    torrent_info meta = meta_tool.load(torrent_path);
    string file_id = meta.file_hash;

    string dest_dir = output_dir.empty() ? data_dir : output_dir;
    filesystem::create_directories(dest_dir);
    string out_file_path = dest_dir + "/" + meta.file_name;

    // Lưu vào database
    torrent_record tr;
    tr.file_id = file_id;
    tr.file_name = meta.file_name;
    tr.file_size = meta.file_size;
    tr.chunk_size = meta.chunk_size;
    tr.num_chunks = meta.num_chunks;
    tr.file_hash = meta.file_hash;
    tr.status = "downloading";
    tr.metadata_path = torrent_path;
    tr.output_path = out_file_path;
    tr.uploaded = 0;
    tr.downloaded = 0;
    tr.added_at = "";
    db->upsert_torrent(tr);

    // Khởi tạo StateManager (toàn bộ false ban đầu hoặc load từ db)
    state_manager->init_file_state(file_id, meta.num_chunks, false);

    // Đăng ký vào UploadServer để chia sẻ lại chunk đã tải
    upload_server->register_file(file_id, out_file_path, meta.chunk_size);

    {
        lock_guard<mutex> lock(active_files_mtx);
        active_files.insert(file_id);
    }

    // Announce ban đầu lên tracker
    announce_request req;
    req.peer_id = peer_id;
    req.ip = "127.0.0.1";
    req.port = port;
    req.file_id = file_id;
    req.bitfield = state_manager->get_bitfield(file_id);
    tracker_client->announce(req);

    // Bắt đầu tải qua DownloadManager
    bool success = download_manager->start_download(meta, out_file_path);
    if (success) {
        log.success("Successfully downloaded file: " + meta.file_name);
        tr.status = "completed";
        db->upsert_torrent(tr);
    } else {
        log.warnning("Download paused or incomplete for file: " + meta.file_name);
    }

    return success;
}
