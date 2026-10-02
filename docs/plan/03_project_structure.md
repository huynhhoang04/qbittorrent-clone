# Cấu trúc Dự án (Project Structure)

Dự án P2P File-Sharing System (BitTorrent Clone) được phát triển bằng C++17.
Công nghệ sử dụng: CMake, POSIX Sockets, Crow HTTP, nlohmann/json, OpenSSL, SQLite3, ncurses.

## 1. Cấu trúc thư mục dự án

```text
qbittorrent-clone/
├── README.md
├── CMakeLists.txt
├── .env.example
├── .gitignore
├── src/
│   ├── common/
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── common/
│   │   │       ├── protocol.hpp
│   │   │       ├── sha256.hpp
│   │   │       ├── metadata.hpp
│   │   │       ├── logger.hpp
│   │   │       └── config.hpp
│   │   └── src/
│   │       ├── protocol.cpp
│   │       ├── sha256.cpp
│   │       ├── metadata.cpp
│   │       ├── logger.cpp
│   │       └── config.cpp
│   ├── tracker/
│   │   ├── CMakeLists.txt
│   │   ├── main.cpp
│   │   ├── tracker_server.hpp
│   │   ├── tracker_server.cpp
│   │   ├── peer_registry.hpp
│   │   └── peer_registry.cpp
│   └── peer/
│       ├── CMakeLists.txt
│       ├── main.cpp
│       ├── peer_node.hpp
│       ├── peer_node.cpp
│       ├── tracker_client.hpp
│       ├── tracker_client.cpp
│       ├── upload_server.hpp
│       ├── upload_server.cpp
│       ├── download_manager.hpp
│       ├── download_manager.cpp
│       ├── file_manager.hpp
│       ├── file_manager.cpp
│       ├── state_manager.hpp
│       ├── state_manager.cpp
│       ├── database.hpp
│       ├── database.cpp
│       ├── tui.hpp
│       └── tui.cpp
├── data/
│   └── .gitkeep
├── tests/
│   ├── CMakeLists.txt
│   ├── test_sha256.cpp
│   ├── test_metadata.cpp
│   ├── test_protocol.cpp
│   ├── test_file_manager.cpp
│   ├── test_peer_registry.cpp
│   ├── test_database.cpp
│   ├── test_concurrent_download.cpp
│   ├── test_fault_recovery.cpp
│   └── test_invalid_data.cpp
├── scripts/
│   ├── start_tracker.sh
│   ├── start_peer.sh
│   ├── demo_full.sh
│   ├── inject_fault.sh
│   └── generate_test_file.sh
├── deploy/
│   ├── Dockerfile.tracker
│   ├── Dockerfile.peer
│   └── docker-compose.yml
└── docs/
    ├── report.pdf
    └── slides.pdf
```

*Lưu ý: Module `common/` sử dụng mô hình `include/common/` để giúp việc import rõ ràng và tránh xung đột tên (ví dụ: `#include "common/protocol.hpp"`).*

## 2. Mô tả chi tiết từng file

### Các file thư mục gốc
- **README.md**: Tài liệu hướng dẫn sử dụng, tổng quan về dự án, hướng dẫn cài đặt và chạy thử.
- **CMakeLists.txt**: File cấu hình CMake chính của dự án, thiết lập các thư viện (OpenSSL, SQLite3, nlohmann_json, Crow, Curses, Threads) và gọi add_subdirectory() cho src và tests.
- **.env.example**: Mẫu file biến môi trường, định nghĩa cấu hình mạng (IP, cổng) hoặc đường dẫn thư mục tạm.
- **.gitignore**: Cấu hình bỏ qua các file build sinh ra (vd: thư mục `build/`, file nhị phân, file cơ sở dữ liệu SQLite cục bộ).

### Thư mục `src/common/` (Cấu trúc dữ liệu và Hàm tiện ích chung)
- **protocol.hpp / .cpp**: Định nghĩa cấu trúc các gói tin giao tiếp giữa Peer-Peer và Tracker-Peer. Cung cấp hàm serialize/deserialize (dùng nlohmann/json).
- **sha256.hpp / .cpp**: Bọc (wrapper) thư viện OpenSSL để tạo hàm băm SHA-256 dùng để băm nội dung file và chunk.
- **metadata.hpp / .cpp**: Xử lý định dạng file `.torrent` ảo (metadata), bao gồm danh sách chunk, kích thước chunk và mã băm tương ứng. Phụ thuộc nlohmann/json.
- **logger.hpp / .cpp**: Tiện ích ghi log an toàn với đa luồng (thread-safe), hỗ trợ in thông báo ra console hoặc file theo nhiều mức độ (INFO, WARN, ERROR).
- **config.hpp / .cpp**: Class Singleton tải và lưu trữ cấu hình tĩnh (đọc từ file hoặc biến môi trường) cho toàn hệ thống.

### Thư mục `src/tracker/` (Tracker Server - Tương đương BitTorrent Tracker)
- **main.cpp**: Điểm vào (entry point) của Tracker. Khởi tạo `TrackerServer`, truyền cổng lắng nghe và cấu hình, chặn và đợi tín hiệu kết thúc.
- **tracker_server.hpp / .cpp**: Máy chủ HTTP quản lý các API cho Tracker (sử dụng thư viện Crow). Xử lý request HTTP (GET, POST) và kết nối tới PeerRegistry.
- **peer_registry.hpp / .cpp**: Cấu trúc dữ liệu lưu trữ danh sách file đang được chia sẻ (swarm) và địa chỉ các Peer tương ứng. Hỗ trợ thêm Peer và loại bỏ Peer timeout.

### Thư mục `src/peer/` (Peer Node - Trạm người dùng cuối)
- **main.cpp**: Khởi chạy Peer, thiết lập cổng, khởi tạo `PeerNode` và giao diện người dùng dựa trên terminal (TUI).
- **peer_node.hpp / .cpp**: Class trung tâm quản lý vòng đời của Peer, khởi tạo và liên kết các module: TrackerClient, UploadServer, DownloadManager, và StateManager.
- **tracker_client.hpp / .cpp**: Đảm nhiệm giao tiếp HTTP (thường dùng cpr hoặc httplib) tới Tracker để báo cáo trạng thái (announce) và lấy danh sách Peer cho các tệp tin.
- **upload_server.hpp / .cpp**: Máy chủ socket POSIX lắng nghe các kết nối TCP đến từ các Peer khác để phục vụ dữ liệu chunk. Xử lý đa luồng cho các kết nối song song.
- **download_manager.hpp / .cpp**: Điều phối quá trình tải một file. Thu thập chunk từ các Peer, yêu cầu tải và kiểm tra mã băm từng chunk. Đảm bảo tải đồng thời hiệu quả.
- **file_manager.hpp / .cpp**: Module I/O disk. Đọc, ghi và ghép các chunk lại với nhau vào file hoàn chỉnh, chia nhỏ file thành chunk khi seed.
- **state_manager.hpp / .cpp**: Quản lý trạng thái nội bộ của Peer như băng thông, danh sách file đang tải, hoàn thành, trạng thái các chunk (Bitfield). Liên kết với Database.
- **database.hpp / .cpp**: Wrapper cho SQLite3. Lưu trữ trạng thái phiên làm việc của Peer (như danh sách download/seed hiện tại, tỉ lệ tải/lên) để khôi phục khi restart ứng dụng.
- **tui.hpp / .cpp**: Giao diện người dùng Terminal UI (dựa trên thư viện ncurses). Hiển thị tiến trình download, thông tin các Peer kết nối và cho phép nhập lệnh quản lý.

### Thư mục `data/`
- **data/**: Thư mục chứa dữ liệu tĩnh. Được sử dụng để lưu các file đang chia sẻ, file cấu hình, cơ sở dữ liệu SQLite sinh ra tự động (.db) và file metadata. File `.gitkeep` giữ thư mục không bị git bỏ qua.

### Thư mục `tests/` (Unit & Integration Tests)
- **CMakeLists.txt**: Cấu hình sử dụng Google Test (thường qua FetchContent) và xây dựng các bài test (test suites).
- Các file test (ví dụ: `test_sha256.cpp`, `test_protocol.cpp`, `test_concurrent_download.cpp`, v.v.): Viết các ca kiểm thử cho từng component từ logic băm, mạng lưới, I/O đến tích hợp download song song và lỗi.

### Thư mục `scripts/` & `deploy/`
- **scripts/**: Các file kịch bản (bash) để chạy nhanh Tracker (`start_tracker.sh`), Peer (`start_peer.sh`), mô phỏng mạng lỗi (`inject_fault.sh`), và tạo file mock data để test (`generate_test_file.sh`).
- **deploy/**: File Docker (Dockerfile) dùng để container hóa ứng dụng và file `docker-compose.yml` để dựng môi trường mô phỏng nhiều node ảo dễ dàng trên một máy.

## 3. CMake Build System

- **Root `CMakeLists.txt`**: Cấu hình dự án chính. Gọi `find_package` cho (OpenSSL, Threads, nlohmann_json, Crow, SQLite3, Curses). Định nghĩa compiler (C++17) và gọi `add_subdirectory()` cho src, tests.
- **`src/common/CMakeLists.txt`**: Định nghĩa thư viện tĩnh (Static Library) `common_lib`. Set thuộc tính `PUBLIC` cho thư mục includes (`include`) và `PUBLIC` link tới OpenSSL::Crypto, nlohmann_json, Threads.
- **`src/tracker/CMakeLists.txt`**: Xây dựng tệp thực thi `tracker_app`. Link thư viện: `common_lib` + `Crow::Crow` + `Threads`.
- **`src/peer/CMakeLists.txt`**: Xây dựng tệp thực thi `peer_app`. Link thư viện: `common_lib` + `SQLite::SQLite3` (hoặc `${SQLite3_LIBRARIES}`) + `Curses::Curses` (hoặc `${CURSES_LIBRARIES}`) + `Threads`.
- **`tests/CMakeLists.txt`**: Thiết lập Google Test qua tính năng FetchContent, tạo các binary test và đăng ký qua `add_test()`.

## 4. Quy trình Build và Chạy

### Cú pháp Build
```bash
mkdir -p build && cd build
cmake ..
cmake --build . -j$(nproc)
```

### Chạy các thành phần
**Chạy Tracker:**
```bash
# Trong thư mục build/
./src/tracker/tracker_app --port 8080
```

**Chạy Peer (Seeder - có sẵn file):**
```bash
# Tại terminal 1
./src/peer/peer_app --tracker http://localhost:8080 --port 5001 --data-dir ../data/peer1
```

**Chạy Peer (Leecher - người tải file):**
```bash
# Tại terminal 2
./src/peer/peer_app --tracker http://localhost:8080 --port 5002 --data-dir ../data/peer2
```

**Chạy Tests:**
```bash
# Trong thư mục build/
ctest --output-on-failure
```

## 5. Deliverables Checklist (Danh sách bàn giao)
Dưới đây là danh sách đánh giá hoàn thành các yêu cầu của học phần:
- [ ] Cấu trúc code module hóa, phân tách common, tracker, peer.
- [ ] Xây dựng hệ thống mạng với thư viện Socket POSIX đa luồng (Upload/Download TCP).
- [ ] Tích hợp API HTTP Tracker với Crow Framework.
- [ ] Serialize gói tin giao tiếp qua nlohmann/json.
- [ ] Băm file và mảnh ghép bằng chuẩn SHA-256 (OpenSSL).
- [ ] Giao diện theo dõi thời gian thực CLI TUI với Ncurses.
- [ ] Lưu trạng thái tiến trình và phục hồi bằng SQLite3.
- [ ] Quản lý cấu hình xây dựng bằng CMake hiện đại (Modern CMake).
- [ ] Container hóa hệ thống với Docker & Docker Compose.
- [ ] Các script bash tự động khởi chạy và sinh file kiểm thử.
- [ ] Báo cáo (PDF) và Slide thuyết trình.
- [ ] Unit Test/Integration Test bao phủ kịch bản bất thường (Tải nhiều nguồn, ngắt mạng, bad data).
