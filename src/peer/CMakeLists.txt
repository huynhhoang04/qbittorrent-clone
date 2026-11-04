cmake_minimum_required(VERSION 3.16)
project(PeerApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

add_library(PeerLib STATIC 
    src/file_manager.cpp 
    src/database.cpp
    src/tracker_client.cpp
    src/state_manager.cpp
    src/upload_server.cpp
    src/download_manager.cpp
    src/peer_node.cpp
)

target_link_libraries(PeerLib PUBLIC CommonLib SQLite::SQLite3 Threads::Threads)
target_include_directories(PeerLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/header)

add_executable(PeerApp src/main.cpp)
target_link_libraries(PeerApp PRIVATE PeerLib CommonLib Threads::Threads)