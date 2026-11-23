#include <bits/stdc++.h>
#include <csignal>
#include <cstdlib>

#include "env.hpp"
#include "logger.hpp"
#include "peer_node.hpp"
#include "tui.hpp"
#include "web_ui.hpp"

using namespace std;

static volatile std::sig_atomic_t g_shutdown_requested = 0;

namespace
{

    void handle_peer_signal(int)
    {
        // The actual shutdown happens on the watcher thread; only the async-signal
        // safe flag is touched here.
        g_shutdown_requested = 1;
    }

    vector<int> parse_chunk_list(const string &value)
    {
        vector<int> chunks;
        stringstream stream(value);
        string item;
        while (getline(stream, item, ','))
        {
            if (item.empty())
            {
                continue;
            }
            try
            {
                const int chunk = stoi(item);
                if (chunk >= 0)
                {
                    chunks.push_back(chunk);
                }
            }
            catch (const exception &)
            {
                // Ignore malformed entries; the CLI validator reports bad flags
                // separately when a value cannot be parsed at all.
            }
        }
        return chunks;
    }

    void print_usage(const char *program)
    {
        cout << "Usage: " << program << " [options]\n";
        cout << "Options:\n";
        cout << "  --tracker <url>          Tracker URL (default: TRACKER_URL or http://localhost:8080)\n";
        cout << "  -p, --port <port>        Upload listening port (default: 6881, 0 = auto)\n";
        cout << "  --data-dir <dir>         Working/storage directory (default: ./data/peer)\n";
        cout << "  --seed <file>            Seed a file (metadata is generated if needed)\n";
        cout << "  --download <torrent>     Download a torrent file\n";
        cout << "  --output-dir <dir>       Destination directory for downloads\n";
        cout << "  --chunk-size <bytes>     Chunk size used when generating metadata\n";
        cout << "  --max-downloads <n>      Maximum concurrent download connections (default 5)\n";
        cout << "  --download-limit-mbps <n> Total download cap in MiB/s (default 0 = unlimited)\n";
        cout << "  --max-uploads <n>        Maximum concurrent upload slots (default 5)\n";
        cout << "  --pipeline-depth <n>     Requests kept in flight per peer (default 3)\n";
        cout << "  --io-timeout-ms <n>      Socket timeout in milliseconds (default 4000)\n";
        cout << "  --chunk-delay-ms <n>     Artificial delay before serving each chunk (tests)\n";
        cout << "  --corrupt-chunks <list>  Comma-separated chunk indexes to corrupt on upload\n";
        cout << "  --announce-ip <ip/host>  Advertised IP or hostname to announce to tracker\n";
        cout << "  --exit-after-download    Exit once the download completes or fails\n";
        cout << "  --tui                    Enable the ncurses dashboard\n";
        cout << "  --headless               Disable the dashboard (default)\n";
        cout << "  --webui                  Enable the web browser dashboard (default port 8081)\n";
        cout << "  --webui-port <port>      Port for the web dashboard (default: 8081)\n";
        cout << "  --help, -h               Show this help\n";
    }

} // namespace

int main(int argc, char *argv[])
{
    Logger log;
    // `.env` is optional; anything already in the real environment wins.
    appenv::load_file(".env");

    string tracker_url = appenv::get_string("TRACKER_URL", "http://localhost:8080");
    int port = appenv::get_int("PEER_PORT", 6881, 0, 65535);
    string data_dir = appenv::get_string("PEER_DATA_DIR", "./data/peer");
    string seed_path;
    string download_path;
    string output_dir;

    peer_node_options options;
    peer_node_options parsed_options;
    // These four were listed in .env.example but never read; they now seed the
    // defaults so the documented file actually does something.
    int max_downloads = appenv::get_int("MAX_ACTIVE_DOWNLOADS", 5, 1, 1024);
    int max_uploads = appenv::get_int("MAX_ACTIVE_UPLOADS", 5, 1, 1024);
    int pipeline_depth = appenv::get_int("PIPELINE_DEPTH", 3, 1, 1024);
    int io_timeout_ms = appenv::get_int("IO_TIMEOUT_MS", 4000, 500, 600000);
    parsed_options.max_active_downloads = max_downloads;
    parsed_options.download_limit_mbps =
        appenv::get_int("DOWNLOAD_LIMIT_MBPS", 0, 0, 1048576);
    parsed_options.max_active_uploads = max_uploads;
    parsed_options.pipeline_depth = pipeline_depth;
    parsed_options.io_timeout_ms = io_timeout_ms;
    parsed_options.webui_port = appenv::get_int("WEBUI_PORT", 8081, 1, 65535);
    size_t chunk_size = 256 * 1024;
    bool option_error = false;

    for (int i = 1; i < argc; ++i)
    {
        const string arg = argv[i];
        auto next_value = [&](const char *name) -> string
        {
            if (i + 1 >= argc)
            {
                cerr << "Missing value for " << name << "\n";
                option_error = true;
                return "";
            }
            return argv[++i];
        };

        if (arg == "--tracker")
        {
            tracker_url = next_value("--tracker");
        }
        else if (arg == "--port" || arg == "-p")
        {
            port = stoi(next_value("--port"));
        }
        else if (arg == "--data-dir")
        {
            data_dir = next_value("--data-dir");
        }
        else if (arg == "--seed")
        {
            seed_path = next_value("--seed");
        }
        else if (arg == "--download")
        {
            download_path = next_value("--download");
        }
        else if (arg == "--output-dir")
        {
            output_dir = next_value("--output-dir");
        }
        else if (arg == "--chunk-size")
        {
            chunk_size = static_cast<size_t>(stoull(next_value("--chunk-size")));
        }
        else if (arg == "--max-downloads")
        {
            parsed_options.max_active_downloads = stoi(next_value("--max-downloads"));
        }
        else if (arg == "--download-limit-mbps")
        {
            const string value = next_value("--download-limit-mbps");
            try {
                size_t consumed = 0;
                const int limit = stoi(value, &consumed);
                if (consumed != value.size() || limit < 0 || limit > 1048576) {
                    throw invalid_argument("invalid download limit");
                }
                parsed_options.download_limit_mbps = limit;
            } catch (const exception&) {
                cerr << "--download-limit-mbps requires an integer from 0 to 1048576 (MiB/s; 0 = unlimited)\n";
                return 2;
            }
        }
        else if (arg == "--max-uploads")
        {
            parsed_options.max_active_uploads = stoi(next_value("--max-uploads"));
        }
        else if (arg == "--pipeline-depth")
        {
            parsed_options.pipeline_depth = stoi(next_value("--pipeline-depth"));
        }
        else if (arg == "--io-timeout-ms")
        {
            parsed_options.io_timeout_ms = stoi(next_value("--io-timeout-ms"));
        }
        else if (arg == "--chunk-delay-ms")
        {
            parsed_options.chunk_delay_ms = stoi(next_value("--chunk-delay-ms"));
        }
        else if (arg == "--corrupt-chunks")
        {
            parsed_options.corrupt_chunks = parse_chunk_list(next_value("--corrupt-chunks"));
        }
        else if (arg == "--exit-after-download")
        {
            parsed_options.exit_after_download = true;
        }
        else if (arg == "--tui")
        {
            parsed_options.enable_tui = true;
        }
        else if (arg == "--announce-ip")
        {
            parsed_options.announce_ip = next_value("--announce-ip");
        }
        else if (arg == "--webui")
        {
            parsed_options.enable_webui = true;
        }
        else if (arg == "--webui-port")
        {
            parsed_options.webui_port = stoi(next_value("--webui-port"));
            parsed_options.enable_webui = true;
        }
        else if (arg == "--help" || arg == "-h")
        {
            print_usage(argv[0]);
            return 0;
        }
        else
        {
            cerr << "Unknown option: " << arg << "\n";
            return 2;
        }
    }

    if (option_error)
    {
        return 2;
    }
    if (chunk_size == 0)
    {
        cerr << "--chunk-size must be greater than zero\n";
        return 2;
    }

    options = parsed_options;
    signal(SIGINT, handle_peer_signal);
#ifdef SIGTERM
    signal(SIGTERM, handle_peer_signal);
#endif

    PeerNode node(tracker_url, port, data_dir);
    node.set_options(options);

    if (!node.start())
    {
        log.err("Failed to start PeerNode");
        return 1;
    }

    unique_ptr<WebUiServer> web_ui;
    if (options.enable_webui)
    {
        web_ui = make_unique<WebUiServer>(node, options.webui_port);
        if (!options.enable_tui)
        {
            Logger::set_sink([&web_ui](log_level level, const string &message) {
                const char* lvl = log_level_name(level);
                const string stamp = log_timestamp();
                cout << "[" << stamp << "] [" << lvl << "] " << message << "\n";
                if (web_ui) {
                    web_ui->add_log("[" + stamp + "] [" + lvl + "] " + message);
                }
            });
        }
        if (web_ui->start())
        {
            log.success("Web UI Dashboard running at http://localhost:" + to_string(options.webui_port));
        }
    }

    bool setup_ok = true;
    if (!seed_path.empty())
    {
        setup_ok = node.seed_file(seed_path, chunk_size) && setup_ok;
    }

    bool download_ok = true;
    if (!download_path.empty())
    {
        if (options.enable_tui || options.enable_webui)
        {
            if (!node.start_download_async(download_path, output_dir))
            {
                log.err("Could not start the download");
                download_ok = false;
            }
        }
        else
        {
            download_ok = node.download_file(download_path, output_dir);
        }
    }

    atomic<bool> watcher_done{false};
    thread watcher([&node, &watcher_done]()
                   {
        while (!watcher_done.load() && !g_shutdown_requested && node.running()) {
            this_thread::sleep_for(chrono::milliseconds(200));
        }
        if (g_shutdown_requested && node.running()) {
            node.stop();
        } });

    int exit_code = 0;
    if (options.enable_tui)
    {
        Tui tui(node);
        exit_code = tui.run();
        // The dashboard returns as soon as the user leaves it, so the transfer
        // result still has to decide the process status.
        if (exit_code == 0 && !download_path.empty() && !node.download_succeeded())
        {
            exit_code = 1;
        }
    }
    else if (!download_path.empty() && options.exit_after_download)
    {
        node.stop();
        exit_code = download_ok ? 0 : 1;
    }
    else
    {
        while (node.running() && !g_shutdown_requested)
        {
            this_thread::sleep_for(chrono::milliseconds(500));
        }
        node.stop();
        if (!download_path.empty() && !download_ok && !g_shutdown_requested)
        {
            exit_code = 1;
        }
        if (!setup_ok)
        {
            exit_code = 1;
        }
    }

    watcher_done.store(true);
    node.stop();
    if (watcher.joinable())
    {
        watcher.join();
    }

    if (web_ui)
    {
        web_ui->stop();
    }
    Logger::clear_sink();

    log.success("PeerApp exited cleanly");
    return exit_code;
}
