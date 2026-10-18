#include <bits/stdc++.h>
#include <csignal>
#include "tracker_server.hpp"
#include "logger.hpp"

using namespace std;

static TrackerServer* g_server = nullptr;

void handle_signal(int signum) {
    Logger log;
    log.warnning("Signal " + to_string(signum) + " received, stopping tracker...");
    if (g_server) g_server->stop();
}

int main(int argc, char* argv[]) {
    Logger log;
    int port = 8080;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            try {
                port = stoi(argv[++i]);
            } catch (...) {
                log.err("Invalid port number: " + string(argv[i]));
                return 1;
            }
        }
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    TrackerServer server(port);
    g_server = &server;

    log.info("Starting TrackerApp on port " + to_string(port));
    server.start();

    g_server = nullptr;
    log.success("TrackerApp exited cleanly");
    return 0;
}