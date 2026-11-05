#include "logger.hpp"

#include <iostream>

using namespace std;

mutex Logger::mtx;
log_sink Logger::sink;

const char* log_level_name(log_level level) {
    switch (level) {
        case log_level::info: return "INFO";
        case log_level::success: return "SUCCESS";
        case log_level::warning: return "WARN";
        case log_level::error: return "ERROR";
    }
    return "INFO";
}

void Logger::set_sink(log_sink new_sink) {
    lock_guard<mutex> lock(mtx);
    sink = std::move(new_sink);
}

void Logger::clear_sink() {
    lock_guard<mutex> lock(mtx);
    sink = nullptr;
}

void Logger::emit(log_level level, const string& msg) {
    log_sink current_sink;
    {
        lock_guard<mutex> lock(mtx);
        current_sink = sink;
    }

    if (current_sink) {
        current_sink(level, msg);
        return;
    }

    static const char* colours[] = {"\033[34m", "\033[32m", "\033[33m", "\033[31m"};
    lock_guard<mutex> lock(mtx);
    cout << colours[static_cast<int>(level)] << "[" << log_level_name(level) << "] "
         << msg << "\033[0m" << endl;
}

void Logger::info(const string& msg) { emit(log_level::info, msg); }

void Logger::success(const string& msg) { emit(log_level::success, msg); }

void Logger::warnning(const string& msg) { emit(log_level::warning, msg); }

void Logger::err(const string& msg) { emit(log_level::error, msg); }
