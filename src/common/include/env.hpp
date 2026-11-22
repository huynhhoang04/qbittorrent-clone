#pragma once

#include <string>

// Minimal environment handling shared by both executables.
//
// The build ships a `.env.example` and asks the operator to copy it to `.env`,
// so the file has to actually be read. Values already present in the process
// environment always win: a container or CI job sets the same variables and
// must not have them overwritten by a checked-in file.
namespace appenv {

// Reads `KEY=VALUE` lines from `path` and exports the ones that are not already
// set. `#` starts a comment, blank lines are ignored, and a single pair of
// surrounding quotes is stripped. Returns false when the file cannot be opened
// (which is normal — `.env` is optional).
bool load_file(const std::string& path);

// Reads `name` from the environment. Returns `fallback` when unset or empty.
std::string get_string(const std::string& name, const std::string& fallback);

// Reads `name` as an integer, clamped to [min_value, max_value]. Returns
// `fallback` when unset, empty, or not a number.
int get_int(const std::string& name, int fallback, int min_value, int max_value);

// Creates or overwrites a variable. Used by load_file; exposed so tests and
// callers can set a value without depending on the platform's setenv spelling.
bool set_string(const std::string& name, const std::string& value);

} // namespace appenv
