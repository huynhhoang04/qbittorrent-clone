# P2P File-Sharing System (BitTorrent Clone)

Dự án cuối kỳ môn **Hệ thống phân tán** (Distributed Systems) — đề tài **DS02**.
Hệ thống chia sẻ tệp ngang hàng (P2P) hoạt động theo mô hình BitTorrent, gồm một **Tracker**
chỉ quản lý siêu dữ liệu và nhiều **Peer** vừa tải vừa phục vụ mảnh dữ liệu.

> Báo cáo kỹ thuật: [`docs/report.pdf`](docs/report.pdf) · Kịch bản demo: [`scripts/demo_full.sh`](scripts/demo_full.sh)

---

## 1. Tổng quan kiến trúc

```
                       ┌──────────────────────────────┐
   announce / peers    │  TrackerApp (HTTP, in-memory)│   announce / peers
   ───────────────────►│  POST /announce              │◄───────────────────
                       │  GET  /peers?file_id=…       │
                       │  GET  /health                │
                       └──────────────────────────────┘
                              (chỉ metadata, KHÔNG bao giờ giữ nội dung tệp)

        Peer 1 (seeder)  ◄──── PIECE / REQUEST (TCP) ────►  Peer 2 (leecher→seeder)
               ▲                                                    ▲
               └────────────── PIECE / REQUEST (TCP) ───────────────┘
                                    Peer 3 (leecher)
```

- **Tracker** không giữ và không chuyển tiếp bất kỳ byte nội dung nào; nó chỉ trả lời
  “ai đang giữ tệp X”. Một tracker chết không làm dừng các luồng tải đang chạy.
- **Peer** là tiến trình lai: một máy chủ TCP phục vụ mảnh, một trình tải đa luồng,
  và một giao diện (CLI / ncurses TUI / Web dashboard).
- **Định danh tệp** là mã băm SHA-256 của toàn bộ nội dung; mỗi mảnh có mã băm riêng
  và được xác minh ngay khi nhận.
- **Giao thức**: HTTP + JSON giữa peer ↔ tracker; TCP thô với khung tin nhị phân
  (`4 byte độ dài · 1 byte loại · payload`, tối đa 8 MB) giữa các peer.

Chi tiết đầy đủ ở mục 7–8 của [`docs/report.pdf`](docs/report.pdf).

---

## 2. Yêu cầu hệ thống

| Thành phần | Phiên bản / ghi chú |
| :--- | :--- |
| Trình biên dịch | GCC ≥ 9 hoặc Clang ≥ 10 (C++17); MSVC/MinGW trên Windows |
| CMake | ≥ 3.16 |
| SQLite3 | ≥ 3.35 (bật được WAL) |
| OpenSSL | ≥ 1.1 (API `EVP`) |
| nlohmann_json | ≥ 3.10 |
| Crow | v1.3.4 (`crow_all.h`) + standalone Asio |
| ncurses | `libncurses-dev` (giao diện TUI) |
| GoogleTest | cho bộ test (`libgtest-dev`) |
| Python 3 | chỉ để dựng lại báo cáo PDF (`scripts/report/`) |

Cài trên Ubuntu/Debian:

```bash
sudo apt-get install -y build-essential cmake libsqlite3-dev libssl-dev \
    nlohmann-json3-dev libncurses-dev libgtest-dev libboost-all-dev libasio-dev curl
sudo wget -q https://github.com/CrowCpp/Crow/releases/download/v1.3.4/crow_all.h \
    -O /usr/local/include/crow.h
```

Trên MSYS2/MinGW64: `mingw-w64-x86_64-{toolchain,cmake,sqlite3,openssl,nlohmann-json,ncurses,gtest,asio,boost}`.

---

## 3. Build và chạy

```bash
# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"          # sinh build/src/peer/PeerApp, build/src/tracker/TrackerApp
```

> Bản `Debug` bật **Address Sanitizer + UBSan** (`-fsanitize=address,undefined`); dùng bản
> `Release` khi demo hoặc đo hiệu năng.

### 3.1. Chạy một swarm thủ công

```bash
# Terminal 1 — tracker
./build/src/tracker/TrackerApp -p 8080

# Terminal 2 — seeder
./build/src/peer/PeerApp --tracker http://127.0.0.1:8080 -p 0 \
    --data-dir ./data/seed --seed ./ubuntu.iso --chunk-size 262144

# Terminal 3 — leecher (nhận ubuntu.iso.torrent.json ngoài kênh P2P trước)
./build/src/peer/PeerApp --tracker http://127.0.0.1:8080 -p 0 \
    --data-dir ./data/leech --download ./ubuntu.iso.torrent.json \
    --output-dir ./downloads
```

Các tuỳ chọn hay dùng (`--help` để xem đủ): `--max-downloads`, `--max-uploads`,
`--pipeline-depth`, `--io-timeout-ms`, `--chunk-delay-ms`, `--corrupt-chunks`,
`--announce-ip`, `--webui --webui-port`, `--tui`, `--exit-after-download`.

### 3.2. Web dashboard

```bash
./build/src/peer/PeerApp --tracker http://127.0.0.1:8080 -p 6881 --webui --webui-port 8081
# Mở http://localhost:8081
```

Dashboard hiển thị lưới piece map (mảnh đã xác minh SHA-256), cho phép upload tệp để seed,
upload `.torrent.json` để tải, và pause/resume.

### 3.3. Export node ra LAN / Internet

Peer tự dò IP card mạng ra ngoài. Có thể chỉ định thủ công:

```bash
./build/src/peer/PeerApp --tracker http://tracker.example.com:8080 \
    --announce-ip 100.x.y.z --webui
# hoặc: PEER_HOST=100.x.y.z ./build/src/peer/PeerApp …
```

---

## 4. Kiểm thử và đo hiệu năng

```bash
# 48 bài unit + integration test (có tracker/peer thật trên loopback)
ctest --test-dir build --output-on-failure

# 7 kịch bản demo tự động — in 1 dòng PASS/FAIL cho mỗi kịch bản
./scripts/demo_full.sh

# Thí nghiệm hiệu năng (cần bản Release), xuất bảng markdown + CSV
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j"$(nproc)"
BENCH_CSV=docs/bench/results.csv ./scripts/bench.sh
```

Kết quả lần chạy trên máy phát triển (8 luồng, tệp 100 MiB, độ trễ 50 ms/mảnh):

| Nguồn | Thời gian | Thông lượng | Tăng tốc |
| ---: | ---: | ---: | ---: |
| 1 | 20,52 s | 4,87 MiB/s | 1,00× |
| 2 | 10,51 s | 9,52 MiB/s | 1,95× |
| 3 | 7,06 s | 14,17 MiB/s | 2,91× |

Số liệu thô và phân tích đầy đủ ở mục 15 của báo cáo.

---

## 5. Bảy kịch bản demo tự động

**Chọn bài theo câu hỏi của giảng viên:** xem [7 file demo chạy riêng](scripts/demo/README.md).
Ví dụ: `bash scripts/demo_full.sh 4` chỉ chạy bài mất nguồn khi đang tải;
không truyền số sẽ chạy cả 7 bài. Đầu mỗi bài in rõ thao tác và điều kiện PASS.

Script tự dựng tracker và các peer thật trên localhost, dùng thư mục tạm,
và chỉ dừng những PID nó tự tạo. Demo hai máy LAN dùng hướng dẫn riêng bên dưới.

| # | Kịch bản | Điều kiện được coi là PASS |
| --- | --- | --- |
| 1 | Đăng ký tracker và khám phá | `/peers` trả về đủ peer cho đúng `file_id` |
| 2 | Tải một nguồn + SHA-256 | Mã băm tệp kết quả khớp nguồn |
| 3 | Tải đa nguồn | Tệp đúng **và** ≥ 2 peer cùng góp mảnh |
| 4 | Seeder chết giữa chừng | Tệp vẫn hoàn tất từ nguồn còn sống, mã băm khớp |
| 5 | Phát hiện mảnh hỏng | Có ≥ 1 mảnh bị phát hiện và tải lại, mã băm khớp |
| 6 | Resume sau khi bị kill | Khởi động lại **chỉ tải phần thiếu**, không tải lại từ đầu |
| 7 | Tracker loại peer quá hạn | Peer chết biến mất khỏi `/peers` trong khoảng 45–75 s |

`scripts/inject_fault.sh <pid> crash|stop` là công cụ dùng trong kịch bản 4, 6, 7.

Danh sách trên có thêm resume và lease nên không khớp một-một với bảy tình huống
DS02 §6.4. Yêu cầu upload khi chưa hoàn tất được kiểm tra trong bộ concurrency;
khi bảo vệ cần trình diễn riêng theo [kịch bản hai máy](docs/demo-2-may.md).

Log kiểm thử và thông tin phiên bản được lưu tại [docs/evidence](docs/evidence/README.md).
Nguồn nội dung/bố cục PDF nằm trong `scripts/report/`; xem
[hướng dẫn dựng báo cáo](scripts/report/README.md) để giữ đúng Times New Roman.

---

## 6. Triển khai bằng Docker

```bash
cd deploy
docker compose up --build      # 1 tracker + 3 peer trên mạng bridge riêng
```

`docker-compose.yml` tạo các hostname `tracker`, `peer-seeder`, `peer-leecher-1`,
`peer-leecher-2`; mỗi peer có volume riêng nên trạng thái SQLite độc lập. Biến `PEER_HOST`
được đặt theo hostname để peer công bố địa chỉ phân giải được trong mạng container, và
`PEER_DATA_DIR=/app/data` trỏ đúng vào volume đã mount.

Dashboard của ba peer được publish ra `localhost:8081`, `localhost:8082`, `localhost:8083`.
Quy trình demo: upload tệp trên dashboard của seeder → tải `.torrent.json` ở đó → upload
file metadata đó trên dashboard của hai leecher.

> Docker daemon phải đang chạy và máy cần mạng để tải Crow/Asio trong bước build.

---

## 7. Cấu hình

Cả hai binary đều đọc `.env` trong thư mục làm việc lúc khởi động:

```bash
cp .env.example .env
```

Quy tắc ưu tiên: **tham số dòng lệnh > biến môi trường thật > `.env` > giá trị mặc định**.
Một biến đã có giá trị **không rỗng** trong môi trường tiến trình sẽ **không** bị `.env` ghi đè, nên cấu hình
của container/CI luôn thắng tệp trong kho.

| Biến | Mặc định | Ý nghĩa |
| --- | --- | --- |
| `TRACKER_PORT` | `8080` | Cổng TrackerApp lắng nghe |
| `TRACKER_URL` | `http://localhost:8080` | URL tracker cho peer |
| `PEER_PORT` | `6881` | Cổng lắng nghe mặc định của peer (`0` = OS cấp) |
| `PEER_HOST` | rỗng → tự dò IP | Địa chỉ/hostname công bố với tracker |
| `PEER_DATA_DIR` | `./data/peer` | Thư mục dữ liệu/DB |
| `WEBUI_PORT` | `8081` | Cổng dashboard (vẫn cần cờ `--webui` để bật) |
| `MAX_ACTIVE_DOWNLOADS` | `5` | Số nguồn tải song song |
| `MAX_ACTIVE_UPLOADS` | `5` | Số slot phục vụ |
| `PIPELINE_DEPTH` | `3` | Số REQUEST chưa trả lời mỗi kết nối |
| `IO_TIMEOUT_MS` | `4000` | Timeout socket |

Giá trị số đọc từ môi trường được kẹp vào khoảng hợp lệ (ví dụ cổng 1–65535, timeout
≥ 500 ms); giá trị sai định dạng sẽ rơi về mặc định thay vì làm chương trình dừng.
`.env` là tùy chọn — thiếu tệp không phải lỗi.

Không commit `.env` — chỉ `.env.example` được đưa vào kho.

---

## 8. Cấu trúc thư mục

```
├── src/
│   ├── common/     protocol, sha256, metadata, logger, net, env  (CommonLib)
│   ├── tracker/    peer_registry, tracker_server, main
│   └── peer/       peer_node, download_manager, upload_server, state_manager,
│                   database, file_manager, tracker_client, tui, web_ui, main
├── tests/          12 tệp, 48 bài (functional / concurrency / fault / invalid-data / env)
├── scripts/        demo_full, bench, inject_fault, start_peer, start_tracker, generate_test_file
├── deploy/         Dockerfile.peer, Dockerfile.tracker, docker-compose.yml
├── docs/           report.pdf, plan/ (kiến trúc, vấn đề phân tán, milestones)
└── .env.example
```

---

## 9. Phân công công việc

| Thành viên | GitHub | Phụ trách chính | Minh chứng |
| --- | --- | --- | --- |
| Hoàng Văn Huynh — B23DCDT131 | [@huynhhoang04](https://github.com/huynhhoang04) | Tracker service (registry, HTTP API, tracker client); M4 peer networking (upload server, download manager, state manager, CLI); Web UI; CI + demo hardening | Issues #1–#8, #21; PR #25, #26 |
| Nguyễn Gia Bách — B23DCVT042 | [@giaback](https://github.com/giaback) | M5 tải song song đa nguồn, connection limiting / choking, full-duplex, concurrency tests; M6 chịu lỗi, tự động hóa demo, TUI ncurses | Issues #9–#14; PR #19, #21 |
| Nguyễn Hữu Trường — B23DCVT424 | [@TruongNguyen2703](https://github.com/TruongNguyen2703) | Docker/Compose, sửa crash tracker, ASan + profiling + dọn mã nguồn, README / .env.example / final test / release tag | Issues #15, #16, #18; PR #22, #23, #24 |

Chi tiết phân công theo cột mốc và bảng đóng góp ở mục 2 của báo cáo.

---

## 10. Quy trình làm việc nhóm

- Mọi đầu việc được quản lý bằng **GitHub Issues**, gắn nhãn theo mốc M3–M7.
- **Không** commit thẳng vào `main`; tạo `feature/<tên-task>`, mở Pull Request, nhờ một
  thành viên khác đọc lại trước khi trộn.
- CI chạy tự động trên Linux (GCC) và Windows (MinGW): build → `ctest` → `demo_full.sh`.

---

## 11. Giấy phép thư viện bên thứ ba

Crow (BSD-3), nlohmann/json (MIT), GoogleTest (BSD-3), SQLite (public domain),
OpenSSL (Apache-2.0). Phần lõi phân tán — tracker, registry, giao thức, chọn mảnh,
retry, phục hồi — do nhóm tự viết.
