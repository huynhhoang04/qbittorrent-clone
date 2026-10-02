cmake_minimum_required(VERSION 4.4.3)
project(CommonLib VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED)

find_package(SQLite3 REQUIRED)

add_library(CommonLib STATIC src/sha256.cpp
                            src/metadata.cpp
                            src/logger.cpp
                            src/protocol.cpp)

target_link_libraries(CommonLib PRIVATE OpenSSL::Crypto)
target_link_libraries(CommonLib PRIVATE Threads::Threads)
target_link_libraries(CommonLib PRIVATE nlohmann_json::nlohmann_json)

target_include_directories(CommonLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)