cmake_minimum_required(VERSION 3.16)
project(PeerApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

if(WIN32)
    # MSYS2 keeps the MinGW ncurses headers under include/ncursesw. Using the
    # POSIX /usr/include copy would mix MSYS and MinGW headers and fail.
    find_path(CURSES_INCLUDE_DIRS ncurses.h
              HINTS ENV MINGW_PREFIX
              PATH_SUFFIXES include/ncursesw include/ncurses)
    find_library(CURSES_LIBRARIES NAMES ncursesw ncurses
                 HINTS ENV MINGW_PREFIX
                 PATH_SUFFIXES lib)
    if(NOT CURSES_INCLUDE_DIRS OR NOT CURSES_LIBRARIES)
        message(FATAL_ERROR "ncurses headers/libraries were not found")
    endif()
else()
    find_package(Curses REQUIRED)
endif()

add_library(PeerLib STATIC 
    src/file_manager.cpp 
    src/database.cpp
    src/tracker_client.cpp
    src/state_manager.cpp
    src/upload_server.cpp
    src/download_manager.cpp
    src/peer_node.cpp
    src/tui.cpp
)

target_link_libraries(PeerLib PUBLIC CommonLib SQLite::SQLite3 Threads::Threads)
target_link_libraries(PeerLib PUBLIC ${CURSES_LIBRARIES})
target_include_directories(PeerLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/header)
target_include_directories(PeerLib PUBLIC ${CURSES_INCLUDE_DIRS})

add_executable(PeerApp src/main.cpp)
target_link_libraries(PeerApp PRIVATE PeerLib CommonLib Threads::Threads)