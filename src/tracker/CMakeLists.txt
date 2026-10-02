cmake_minimum_required(VERSION 4.4.3)
project(TrackerApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(TrackerApp src/main.cpp)

target_link_libraries(TrackerApp PRIVATE CommonLib)