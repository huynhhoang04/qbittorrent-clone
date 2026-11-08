cmake_minimum_required(VERSION 3.16)

project(MainApp VERSION 1.0.0 LANGUAGES CXX)

# Thiết lập chuẩn C++ (Nên để ngay dưới project)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# [Issue 16] - Code Cleanup: Bật tối đa cảnh báo compiler
add_compile_options(-Wall -Wextra)

# [Issue 16] - Profiling: Bật Address Sanitizer (ASan) để bắt lỗi rò rỉ bộ nhớ
# Lưu ý: Khi nào cần test Thread Sanitizer (TSan) thì sửa chữ 'address' thành 'thread'
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined -g -O1")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address,undefined -g -O1")

add_subdirectory(src/common)
add_subdirectory(src/tracker)
add_subdirectory(src/peer)

enable_testing()
add_subdirectory(tests)