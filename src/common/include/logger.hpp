#pragma once

#include <bits/stdc++.h>
#include <functional>
#include <mutex>
#include <string>

using namespace std;

enum class log_level { info, success, warning, error };

// A sink receives already-formatted, colour-free log lines. When no sink is
// installed the logger writes to stdout, which keeps the existing CLI output.
using log_sink = std::function<void(log_level, const std::string&)>;

class Logger {
    private:
        static mutex mtx;
        static log_sink sink;

    public:
        static void set_sink(log_sink new_sink);
        static void clear_sink();

        void info(const string& msg);
        void success(const string& msg);
        void warnning(const string& msg);
        void err(const string& msg);

    private:
        void emit(log_level level, const string& msg);
};

const char* log_level_name(log_level level);
