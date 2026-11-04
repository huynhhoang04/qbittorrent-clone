#pragma once

#include <memory>

#include "peer_node.hpp"

// ncurses dashboard for a running PeerNode. The header intentionally does not
// expose ncurses types so the rest of the application can be compiled without
// pulling curses macros into unrelated translation units.
class Tui {
    public:
        explicit Tui(PeerNode& node);
        ~Tui();

        // Runs the dashboard until the user presses 'q' or the node stops.
        int run();

    private:
        struct impl;
        PeerNode& node;
        // Shared rather than unique: the logger holds the sink, and a network
        // thread may still append to it while the dashboard is being destroyed.
        std::shared_ptr<impl> state;
};
