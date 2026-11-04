find_package(GTest REQUIRED CONFIG)

add_executable(TestAll 
    test_sha256.cpp 
    test_metadata.cpp 
    test_file_manager.cpp
    test_protocol.cpp
    test_database.cpp
    test_peer_registry.cpp
    test_tracker_server.cpp
    test_state_manager.cpp
    test_concurrent_download.cpp
    test_invalid_data.cpp
    test_fault_recovery.cpp
)

target_link_libraries(TestAll 
    PRIVATE 
    GTest::gtest_main 
    CommonLib 
    PeerLib
    TrackerLib
)

include(GoogleTest)
gtest_discover_tests(TestAll)

# The crash-recovery test must kill and restart a genuine peer process, so the
# test binary needs the location of the real executable rather than a stub.
target_compile_definitions(TestAll PRIVATE
    PEER_APP_PATH="$<TARGET_FILE:PeerApp>")
