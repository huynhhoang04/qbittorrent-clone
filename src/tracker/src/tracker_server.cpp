#include <bits/stdc++.h>
#include "tracker_server.hpp"
#include "logger.hpp"
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

static unique_ptr<TrackerServer> default_server = nullptr;
static mutex server_mtx;

void start_tracker_server(int port) {
    lock_guard<mutex> lock(server_mtx);
    if (!default_server) {
        default_server = make_unique<TrackerServer>(port);
    }
    default_server->start();
}

void stop_tracker_server() {
    lock_guard<mutex> lock(server_mtx);
    if (default_server) {
        default_server->stop();
    }
}

TrackerServer::TrackerServer(int port) : port(port), running(false) {
    app.loglevel(crow::LogLevel::Warning);
    setup_routes();
}

TrackerServer::~TrackerServer() {
    stop();
}

void TrackerServer::setup_routes() {
    CROW_ROUTE(app, "/announce").methods(crow::HTTPMethod::Post)(
        [this](const crow::request& req) {
            Logger log;
            try {
                json body = json::parse(req.body);
                announce_request data = body.get<announce_request>();

                if (data.peer_id.empty() || data.file_id.empty() || data.port <= 0) {
                    log.err("Invalid announce request: missing required fields");
                    return crow::response(400, "application/json", "{\"error\":\"Missing or invalid required fields\"}");
                }

                peer_entry entry;
                entry.peer_id = data.peer_id;
                entry.ip = data.ip.empty() ? req.remote_ip_address : data.ip;
                entry.port = data.port;
                entry.bitfield = data.bitfield;
                entry.last_seen = chrono::steady_clock::now();

                registry.add_or_update(data.file_id, entry);
                log.info("Announce success from " + entry.peer_id + " for file " + data.file_id);
                return crow::response(200, "application/json", "{\"status\":\"ok\"}");
            } catch (const exception& e) {
                log.err("Announce error: " + string(e.what()));
                return crow::response(400, "application/json", "{\"error\":\"Invalid JSON format\"}");
            }
        }
    );

    CROW_ROUTE(app, "/peers").methods(crow::HTTPMethod::Get)(
        [this](const crow::request& req) {
            Logger log;
            const char* fid = req.url_params.get("file_id");
            if (!fid || strlen(fid) == 0) {
                log.err("Missing file_id parameter in /peers request");
                return crow::response(400, "application/json", "{\"error\":\"Missing file_id query parameter\"}");
            }

            string file_id = fid;
            auto peers = registry.get_peers(file_id);

            json arr = json::array();
            for (const auto& p : peers) {
                peer_info_response info;
                info.peer_id = p.peer_id;
                info.ip = p.ip;
                info.port = p.port;
                info.bitfield = p.bitfield;
                arr.push_back(info);
            }

            log.info("Query peers for " + file_id + ": found " + to_string(peers.size()));
            return crow::response(200, "application/json", arr.dump());
        }
    );

    CROW_ROUTE(app, "/health").methods(crow::HTTPMethod::Get)(
        [this](const crow::request&) {
            json j;
            j["status"] = "ok";
            j["total_peers"] = registry.total_peers();
            return crow::response(200, "application/json", j.dump());
        }
    );
}

void TrackerServer::start() {
    Logger log;
    if (running.exchange(true)) return;

    log.info("Tracker server starting on port " + to_string(port));

    cleanup_thread = thread([this]() {
        while (running.load()) {
            unique_lock<mutex> lock(cv_mtx);
            if (cv.wait_for(lock, chrono::seconds(10), [this] { return !running.load(); })) {
                break;
            }
            registry.cleanup_stale(45);
        }
    });

    app.port(port).multithreaded().run();
}

void TrackerServer::stop() {
    Logger log;
    if (!running.exchange(false)) return;

    log.info("Stopping tracker server...");
    cv.notify_all();
    if (cleanup_thread.joinable()) {
        cleanup_thread.join();
    }
    app.stop();
    log.success("Tracker server stopped");
}

PeerRegistry& TrackerServer::get_registry() {
    return registry;
}

int TrackerServer::get_port() const {
    return port;
}

bool TrackerServer::is_running() const {
    return running.load();
}
