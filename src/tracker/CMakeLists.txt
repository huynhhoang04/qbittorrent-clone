cmake_minimum_required(VERSION 4.4.3)
project(TrackerApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

if(NOT TARGET Crow::Crow)
    find_path(CROW_INCLUDE_DIR crow.h)
    find_path(ASIO_INCLUDE_DIR asio.hpp)
    if(CROW_INCLUDE_DIR)
        add_library(Crow::Crow INTERFACE IMPORTED)
        target_include_directories(Crow::Crow INTERFACE ${CROW_INCLUDE_DIR})
        if(ASIO_INCLUDE_DIR)
            target_include_directories(Crow::Crow INTERFACE ${ASIO_INCLUDE_DIR})
            target_compile_definitions(Crow::Crow INTERFACE ASIO_STANDALONE)
        endif()
        target_link_libraries(Crow::Crow INTERFACE Threads::Threads)
    else()
        find_package(Crow REQUIRED)
    endif()
endif()

add_library(TrackerLib STATIC 
    src/peer_registry.cpp 
    src/tracker_server.cpp
)

target_link_libraries(TrackerLib PRIVATE CommonLib Crow::Crow Threads::Threads)
target_include_directories(TrackerLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/header)

add_executable(TrackerApp src/main.cpp)
target_link_libraries(TrackerApp PRIVATE TrackerLib CommonLib)