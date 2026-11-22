// The `.env` loader is the only piece of configuration plumbing, and a bug in
// it would silently change a deployment's port or data directory. These tests
// pin the parsing rules and, more importantly, that the real environment wins.

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "env.hpp"

using namespace std;

namespace {

string write_temp_env(const string &contents, const string &name) {
    const string dir = "env_test_" + name;
    filesystem::create_directories(dir);
    const string path = dir + "/.env";
    ofstream out(path);
    out << contents;
    out.close();
    return path;
}

void clear_var(const string &name) {
#ifdef _WIN32
    _putenv_s(name.c_str(), "");
#else
    unsetenv(name.c_str());
#endif
}

} // namespace

TEST(EnvTest, ParsesPlainKeyValueLines) {
    clear_var("QBT_TEST_ALPHA");
    const string path = write_temp_env("QBT_TEST_ALPHA=4242\n", "alpha");

    ASSERT_TRUE(appenv::load_file(path));
    EXPECT_EQ(appenv::get_int("QBT_TEST_ALPHA", 0, 0, 100000), 4242);
    clear_var("QBT_TEST_ALPHA");
}

TEST(EnvTest, IgnoresCommentsBlankLinesAndMalformedEntries) {
    clear_var("QBT_TEST_BETA");
    clear_var("QBT_TEST_GAMMA");
    const string path = write_temp_env(
        "# a comment\n"
        "\n"
        "   \n"
        "QBT_TEST_BETA=7\n"
        "no_equals_sign_here\n"
        "=orphan_value\n"
        "QBT_TEST_GAMMA=\"quoted\"\n",
        "beta");

    ASSERT_TRUE(appenv::load_file(path));
    EXPECT_EQ(appenv::get_int("QBT_TEST_BETA", 0, 0, 100), 7);
    // Surrounding quotes are stripped, so the value is exactly `quoted`.
    EXPECT_EQ(appenv::get_string("QBT_TEST_GAMMA", ""), "quoted");
    clear_var("QBT_TEST_BETA");
    clear_var("QBT_TEST_GAMMA");
}

TEST(EnvTest, RealEnvironmentBeatsTheFile) {
    // This is the property a container or CI job depends on: setting a variable
    // for the process must not be undone by a checked-in .env.
    appenv::set_string("QBT_TEST_DELTA", "from-process");
    const string path = write_temp_env("QBT_TEST_DELTA=from-file\n", "delta");

    ASSERT_TRUE(appenv::load_file(path));
    EXPECT_EQ(appenv::get_string("QBT_TEST_DELTA", ""), "from-process");
    clear_var("QBT_TEST_DELTA");
}

TEST(EnvTest, MissingFileIsNotAnError) {
    // `.env` is optional, so an absent file must return false without throwing.
    EXPECT_FALSE(appenv::load_file("definitely/not/here/.env"));
}

TEST(EnvTest, IntegerReaderFallsBackAndClamps) {
    clear_var("QBT_TEST_EPSILON");
    EXPECT_EQ(appenv::get_int("QBT_TEST_EPSILON", 11, 0, 100), 11);

    appenv::set_string("QBT_TEST_EPSILON", "not-a-number");
    EXPECT_EQ(appenv::get_int("QBT_TEST_EPSILON", 11, 0, 100), 11);

    appenv::set_string("QBT_TEST_EPSILON", "42oops");
    EXPECT_EQ(appenv::get_int("QBT_TEST_EPSILON", 11, 0, 100), 11);

    appenv::set_string("QBT_TEST_EPSILON", "1.5");
    EXPECT_EQ(appenv::get_int("QBT_TEST_EPSILON", 11, 0, 100), 11);

    appenv::set_string("QBT_TEST_EPSILON", " 42 \t");
    EXPECT_EQ(appenv::get_int("QBT_TEST_EPSILON", 11, 0, 100), 42);

    appenv::set_string("QBT_TEST_EPSILON", "500");
    EXPECT_EQ(appenv::get_int("QBT_TEST_EPSILON", 11, 0, 100), 100);

    appenv::set_string("QBT_TEST_EPSILON", "-5");
    EXPECT_EQ(appenv::get_int("QBT_TEST_EPSILON", 11, 0, 100), 0);

    clear_var("QBT_TEST_EPSILON");
}

TEST(EnvTest, EmptyStringFallsBack) {
    appenv::set_string("QBT_TEST_ZETA", "");
    EXPECT_EQ(appenv::get_string("QBT_TEST_ZETA", "fallback"), "fallback");
    clear_var("QBT_TEST_ZETA");
}
