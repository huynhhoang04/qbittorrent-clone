#include <bits/stdc++.h>
#include <csignal>
#include "peer_node.hpp"
#include "logger.hpp"

using namespace std;

static PeerNode* g_peer_node = nullptr;

void handle_peer_signal(int signum) {
    Logger log;
    log.warnning("Signal " + to_string(signum) + " received, stopping peer node...");
    if (g_peer_node) {
        g_peer_node->stop();
    }
}

int main(int argc, char* argv[]) {
    Logger log;
    string tracker_url = "http://localhost:8080";
    int port = 6881;
    string data_dir = "./data/peer";
    string seed_path = "";
    string download_path = "";

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--tracker" && i + 1 < argc) {
            tracker_url = argv[++i];
        } else if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            port = stoi(argv[++i]);
        } else if (arg == "--data-dir" && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (arg == "--seed" && i + 1 < argc) {
            seed_path = argv[++i];
        } else if (arg == "--download" && i + 1 < argc) {
            download_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            cout << "Usage: " << argv[0] << " [options]\n";
            cout << "Options:\n";
            cout << "  --tracker <url>        Tracker URL (default: http://localhost:8080)\n";
            cout << "  -p, --port <port>      Upload listening port (default: 6881)\n";
            cout << "  --data-dir <dir>       Working/Storage directory (default: ./data/peer)\n";
            cout << "  --seed <file>          Run in Seeder mode for file\n";
            cout << "  --download <torrent>   Run in Leecher mode to download torrent\n";
            return 0;
        }
    }

    signal(SIGINT, handle_peer_signal);
    signal(SIGTERM, handle_peer_signal);

    PeerNode node(tracker_url, port, data_dir);
    g_peer_node = &node;

    if (!node.start()) {
        log.err("Failed to start PeerNode");
        return 1;
    }

    if (!seed_path.empty()) {
        log.info("Running as Seeder for: " + seed_path);
        node.seed_file(seed_path);
        // Giữ node chạy để phục vụ upload
        while (node.running()) {
            this_thread::sleep_for(chrono::seconds(1));
        }
    } else if (!download_path.empty()) {
        log.info("Running as Downloader for: " + download_path);
        bool res = node.download_file(download_path);
        if (res) {
            log.success("Download finished! Continuing to seed...");
            while (node.running()) {
                this_thread::sleep_for(chrono::seconds(1));
            }
        }
    } else {
        log.info("PeerNode running in idle mode. Press Ctrl+C to stop.");
        while (node.running()) {
            this_thread::sleep_for(chrono::seconds(1));
        }
    }

    g_peer_node = nullptr;
    log.success("PeerApp exited cleanly");
    return 0;
}