#include <bits/stdc++.h>
#include <csignal>
#include <cstdlib>

#include "logger.hpp"
#include "peer_node.hpp"
#include "tui.hpp"

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
        cout << "  --max-uploads <n>        Maximum concurrent upload slots (default 5)\n";
        cout << "  --pipeline-depth <n>     Requests kept in flight per peer (default 3)\n";
        cout << "  --io-timeout-ms <n>      Socket timeout in milliseconds (default 4000)\n";
        cout << "  --chunk-delay-ms <n>     Artificial delay before serving each chunk (tests)\n";
        cout << "  --corrupt-chunks <list>  Comma-separated chunk indexes to corrupt on upload\n";
        cout << "  --exit-after-download    Exit once the download completes or fails\n";
        cout << "  --tui                    Enable the ncurses dashboard\n";
        cout << "  --headless               Disable the dashboard (default)\n";
        cout << "  --help, -h               Show this help\n";
    }

} // namespace

int main(int argc, char *argv[])
{
    Logger log;
    const char *tracker_url_env = std::getenv("TRACKER_URL");
    string tracker_url = tracker_url_env && *tracker_url_env
                             ? tracker_url_env
                             : "http://localhost:8080";
    int port = 6881;
    string data_dir = "./data/peer";
    string seed_path;
    string download_path;
    string output_dir;

    peer_node_options options;
    peer_node_options parsed_options;
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
        else if (arg == "--headless")
        {
            parsed_options.enable_tui = false;
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

    bool setup_ok = true;
    if (!seed_path.empty())
    {
        setup_ok = node.seed_file(seed_path, chunk_size) && setup_ok;
    }

    bool download_ok = true;
    if (!download_path.empty())
    {
        if (options.enable_tui)
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

    log.success("PeerApp exited cleanly");
    return exit_code;
}
