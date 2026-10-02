find_package(GTest REQUIRED)

add_executable(TestAll 
    test_sha256.cpp 
    test_metadata.cpp 
    test_file_manager.cpp
    test_protocol.cpp
    test_database.cpp
)

target_link_libraries(TestAll 
    PRIVATE 
    GTest::gtest_main 
    CommonLib 
    PeerLib
)

include(GoogleTest)
gtest_discover_tests(TestAll)
