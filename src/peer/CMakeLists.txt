cmake_minimum_required(VERSION 4.4.3)
project(PeerApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)

add_library(PeerLib STATIC src/file_manager.cpp src/database.cpp)

add_executable(PeerApp src/main.cpp)

target_link_libraries(PeerLib PRIVATE CommonLib SQLite::SQLite3)
target_link_libraries(PeerApp PRIVATE PeerLib CommonLib)

target_include_directories(PeerLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/header)