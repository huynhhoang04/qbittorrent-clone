#include "tui.hpp"

#include <ncurses.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <mutex>
#include <sstream>

#include "logger.hpp"

using namespace std;
using namespace std::chrono;

namespace {

string format_bytes(uint64_t bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    ostringstream out;
    if (unit == 0) {
        out << static_cast<uint64_t>(value) << " B";
    } else {
        out.setf(ios::fixed);
        out.precision(value >= 100.0 ? 0 : 1);
        out << value << " " << units[unit];
    }
    return out.str();
}

string format_speed(double bytes_per_second) {
    return format_bytes(static_cast<uint64_t>(std::max(0.0, bytes_per_second))) + "/s";
}

string progress_bar(double percent, int width) {
    percent = std::max(0.0, std::min(100.0, percent));
    const int filled =
        static_cast<int>((percent / 100.0) * static_cast<double>(width) + 0.5);
    string bar = "[";
    bar.append(static_cast<size_t>(filled), '#');
    bar.append(static_cast<size_t>(width - filled), '-');
    bar += "]";
    return bar;
}

string truncate(const string& text, size_t max_length) {
    if (text.size() <= max_length) {
        return text;
    }
    if (max_length <= 3) {
        return text.substr(0, max_length);
    }
    return text.substr(0, max_length - 3) + "...";
}

} // namespace

struct Tui::impl {
    mutex log_mtx;
    deque<string> logs;
    steady_clock::time_point last_sample = steady_clock::now();
    uint64_t last_download_bytes = 0;
    uint64_t last_upload_bytes = 0;
    double download_speed = 0.0;
    double upload_speed = 0.0;
    bool paused = false;

    void append_log(log_level level, const string& message) {
        lock_guard<mutex> lock(log_mtx);
        ostringstream line;
        line << "[" << log_level_name(level) << "] " << message;
        logs.push_back(line.str());
        while (logs.size() > 200) {
            logs.pop_front();
        }
    }
};

Tui::Tui(PeerNode& node) : node(node), state(make_unique<impl>()) {}

Tui::~Tui() {
    Logger::clear_sink();
    if (isendwin() == FALSE) {
        endwin();
    }
}

int Tui::run() {
    if (initscr() == nullptr) {
        return 1;
    }

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_CYAN, -1);
        init_pair(2, COLOR_GREEN, -1);
        init_pair(3, COLOR_YELLOW, -1);
        init_pair(4, COLOR_RED, -1);
    }

    Logger::set_sink([&self = *state](log_level level, const string& message) {
        self.append_log(level, message);
    });

    bool quit = false;
    while (!quit && node.running()) {
        const int key = getch();
        if (key == 'q' || key == 'Q') {
            quit = true;
            break;
        }
        if (key == 'p' || key == 'P') {
            node.pause_download();
            state->paused = true;
            state->append_log(log_level::warning, "Download paused by user");
        }
        if (key == 'r' || key == 'R') {
            node.resume_download(true);
            state->paused = false;
            state->append_log(log_level::info, "Download resumed by user");
        }

        const peer_node_status status = node.get_status();

        const auto now = steady_clock::now();
        const double seconds =
            duration_cast<duration<double>>(now - state->last_sample).count();
        if (seconds >= 0.2) {
            const double down_delta = static_cast<double>(
                status.download.bytes_downloaded - state->last_download_bytes);
            const double up_delta =
                static_cast<double>(status.uploaded_bytes - state->last_upload_bytes);
            const double smoothing = 0.6;
            state->download_speed =
                smoothing * state->download_speed + (1.0 - smoothing) * down_delta / seconds;
            state->upload_speed =
                smoothing * state->upload_speed + (1.0 - smoothing) * up_delta / seconds;
            state->last_download_bytes = status.download.bytes_downloaded;
            state->last_upload_bytes = status.uploaded_bytes;
            state->last_sample = now;
        }

        int rows = 0;
        int cols = 0;
        getmaxyx(stdscr, rows, cols);
        erase();

        int row = 0;
        auto line = [&](const string& text) {
            if (row < rows - 1) {
                mvaddnstr(row, 0, text.c_str(), cols - 1);
                ++row;
            }
        };

        if (has_colors()) {
            attron(COLOR_PAIR(1) | A_BOLD);
        }
        line("BitTorrent Clone - Peer Dashboard");
        if (has_colors()) {
            attroff(COLOR_PAIR(1) | A_BOLD);
        }
        line("Peer ID: " + status.peer_id + "   Port: " + to_string(status.port) +
             "   Tracker: " + (status.tracker_online ? "ONLINE" : "OFFLINE") +
             "   Download: " + (status.download_active ? "ACTIVE" : "IDLE") +
             (state->paused ? " (PAUSED)" : ""));
        line("Speed  down " + format_speed(state->download_speed) + "   up " +
             format_speed(state->upload_speed) + "   upload slots " +
             to_string(status.active_upload_slots) + "/" +
             to_string(node.get_options().max_active_uploads) + "   peak " +
             to_string(status.peak_upload_slots));
        line("");

        line("Torrents");
        if (status.torrents.empty()) {
            line("  (no torrents registered yet)");
        }
        for (const auto& torrent : status.torrents) {
            ostringstream text;
            text << "  " << truncate(torrent.file_name, 28) << "  "
                 << format_bytes(torrent.file_size) << "  " << torrent.mode << "  "
                 << progress_bar(torrent.progress, 24) << " ";
            text.setf(ios::fixed);
            text.precision(1);
            text << torrent.progress << "% (" << torrent.completed_chunks << "/"
                 << torrent.num_chunks << " chunks)";
            line(text.str());
        }
        line("");

        line("Download peers (" + to_string(status.download_peers.size()) + ")");
        if (status.download_peers.empty()) {
            line("  (none)");
        }
        for (const auto& peer : status.download_peers) {
            ostringstream text;
            text << "  " << truncate(peer.peer_id, 18) << " " << peer.ip << ":"
                 << peer.port << "  "
                 << (peer.connected ? (peer.choked ? "CHOKED" : "UNCHOKED")
                                    : "DISCONNECTED")
                 << "  chunks " << peer.chunks_downloaded << "  "
                 << format_bytes(peer.bytes_downloaded);
            if (!peer.last_error.empty()) {
                text << "  (" << truncate(peer.last_error, 32) << ")";
            }
            line(text.str());
        }
        line("");

        line("Upload sessions (" + to_string(status.upload_peers.size()) + ")");
        if (status.upload_peers.empty()) {
            line("  (none)");
        }
        for (const auto& peer : status.upload_peers) {
            ostringstream text;
            text << "  " << truncate(peer.peer_id, 18) << " " << peer.ip << ":"
                 << peer.port << "  " << (peer.choked ? "CHOKED" : "UNCHOKED")
                 << "  chunks " << peer.chunks_uploaded << "  "
                 << format_bytes(peer.bytes_uploaded);
            line(text.str());
        }
        line("");

        line("Log (last lines)");
        {
            lock_guard<mutex> lock(state->log_mtx);
            const int available = std::max(0, rows - row - 2);
            const int count =
                std::min<int>(available, static_cast<int>(state->logs.size()));
            for (int i = 0; i < count; ++i) {
                line("  " + truncate(state->logs[state->logs.size() - count + i],
                                     static_cast<size_t>(std::max(10, cols - 4))));
            }
        }

        if (rows > 1) {
            mvaddnstr(rows - 1, 0,
                      "q: quit   p: pause   r: resume   (keys are case-insensitive)",
                      cols - 1);
        }
        refresh();
        this_thread::sleep_for(milliseconds(150));
    }

    Logger::clear_sink();
    endwin();
    if (quit) {
        node.stop();
    }
    return 0;
}
