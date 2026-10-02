cmake_minimum_required(VERSION 4.4.3)

project(MainApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src/common)
add_subdirectory(src/tracker)
add_subdirectory(src/peer)

enable_testing()
add_subdirectory(tests)