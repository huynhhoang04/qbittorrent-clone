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
