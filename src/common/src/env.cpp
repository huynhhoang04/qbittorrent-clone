#include "env.hpp"

#include <cstdlib>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <stdlib.h>
#endif

using namespace std;

namespace {

string trim(const string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

string unquote(const string& value) {
    if (value.size() >= 2) {
        const char front = value.front();
        const char back = value.back();
        if ((front == '"' && back == '"') || (front == '\'' && back == '\'')) {
            return value.substr(1, value.size() - 2);
        }
    }
    return value;
}

} // namespace

namespace appenv {

bool set_string(const string& name, const string& value) {
    if (name.empty()) {
        return false;
    }
#ifdef _WIN32
    return _putenv_s(name.c_str(), value.c_str()) == 0;
#else
    return setenv(name.c_str(), value.c_str(), 1) == 0;
#endif
}

string get_string(const string& name, const string& fallback) {
    const char* value = getenv(name.c_str());
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    return value;
}

int get_int(const string& name, int fallback, int min_value, int max_value) {
    const char* value = getenv(name.c_str());
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    try {
        const int parsed = stoi(value);
        if (parsed < min_value) {
            return min_value;
        }
        if (parsed > max_value) {
            return max_value;
        }
        return parsed;
    } catch (const exception&) {
        return fallback;
    }
}

bool load_file(const string& path) {
    if (path.empty()) {
        return false;
    }
    ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    string line;
    while (getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto equals = line.find('=');
        if (equals == string::npos) {
            continue;
        }
        const string key = trim(line.substr(0, equals));
        const string value = unquote(trim(line.substr(equals + 1)));
        if (key.empty()) {
            continue;
        }
        // The real environment wins over the file, so a container or CI job
        // that sets the same variable is never silently overridden.
        const char* existing = getenv(key.c_str());
        if (existing != nullptr && *existing != '\0') {
            continue;
        }
        set_string(key, value);
    }
    return true;
}

} // namespace appenv
