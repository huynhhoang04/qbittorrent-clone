# P2P File-Sharing System (BitTorrent Clone)

Đây là dự án cuối kỳ môn Hệ thống phân tán (Distributed Systems) - Nhóm 3 người.
Dự án là một hệ thống chia sẻ file ngang hàng (P2P) hoạt động tương tự như BitTorrent, bao gồm Tracker Server và các Peer Node.

## Yêu cầu Hệ thống
- C++17
- CMake (>= 3.10)
- SQLite3
- OpenSSL
- nlohmann_json
- Crow (HTTP framework)
- ncurses (cho giao diện TUI)

## Hướng dẫn Build (Dành cho Dev)

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

Chạy toàn bộ Unit Tests:
```bash
cd build
ctest --output-on-failure
# Hoặc chạy trực tiếp file test:
./tests/TestAll
```

Chạy toàn bộ 7 kịch bản Demo tự động:
```bash
./scripts/demo_full.sh
```

---

## 🌐 Khởi Chạy Web UI Dashboard & Export Node Ra LAN / Internet

### 1. Bật Tracker Server
```bash
./build/src/tracker/TrackerApp -p 8080
```

### 2. Bật Peer Node kèm Web UI Dashboard (Trình duyệt)
Mở giao diện Web tại `http://localhost:8081` (hoặc cổng bất kỳ tùy chọn bằng `--webui-port`):
```bash
./build/src/peer/PeerApp --tracker http://localhost:8080 -p 6881 --webui --webui-port 8081
```

- **Piece Map Grid**: Quan sát từng chunk xanh/xám được kiểm tra toàn vẹn SHA-256 theo thời gian thực.
- **Thao tác chuột**: Upload file trực tiếp từ trình duyệt để tự động tạo `.torrent.json` và Seed, hoặc upload `.torrent.json` để tải về máy.
- **Quản trị Swarm**: Bật/tắt Pause, Resume, xem tốc độ Down/Up mượt mà.

### 3. Export Node ra LAN / Internet (WAN)
Hệ thống đã hỗ trợ tự động nhận diện IP card mạng thật và giải phân giải DNS/Hostname:
- Tự động nhận diện Outbound IP card mạng (không cần cấu hình).
- Hoặc chủ động khai báo IP/Domain công khai:
  ```bash
  ./build/src/peer/PeerApp --tracker http://my-tracker-domain.com:8080 --announce-ip 100.x.y.z --webui
  ```
  *(Cũng có thể dùng biến môi trường `PEER_HOST=100.x.y.z`).*

---

## 🚀 Hướng Dẫn Quy Trình Làm Việc Nhóm (Team Workflow)

Vì dự án đang trong giai đoạn phát triển song song (Milestone 3, 4, 5...) và có 3 thành viên, chúng ta sẽ áp dụng **Git Flow cơ bản** để tránh việc code bị conflict hoặc ghi đè lên nhau.

### 1. Nhận Task (Assign Task)
- Mọi đầu việc sẽ được quản lý trên phần **Issues** của GitHub.
- Trước khi làm bất cứ tính năng gì, hãy vào phần Issues, tìm task tương ứng (hoặc tự tạo Issue mới nếu chưa có).
- Nhấn **Assign yourself** để thông báo cho 2 người còn lại biết là bạn đang làm phần này.

### 2. Tạo Branch (Nhánh mới)
- **TUYỆT ĐỐI KHÔNG** code trực tiếp và push thẳng lên nhánh `main`.
- Trước khi bắt đầu code task mới, hãy tạo một nhánh mới từ `main`:
```bash
git checkout main
git pull origin main # Cập nhật code mới nhất từ mọi người
git checkout -b feature/<tên-task-viết-liền-không-dấu>
# Ví dụ: git checkout -b feature/tracker-service
```

### 3. Commit Code
- Code xong một phần chức năng (hoặc hết ngày làm việc), hãy commit với thông điệp rõ ràng:
```bash
git add .
git commit -m "feat: Thêm chức năng đăng ký Peer trên Tracker (#IssueID)"
```

### 4. Push và Tạo Pull Request (PR)
- Push nhánh của bạn lên Github:
```bash
git push origin feature/<tên-task>
```
- Lên Github, tạo một **Pull Request (PR)** từ nhánh `feature/...` vào nhánh `main`.
- Nhờ 1 trong 2 người còn lại review code, nếu mọi thứ OK thì bấm **Merge** vào `main`.

### 5. Cập nhật code mới
- Trong lúc bạn đang làm, nếu có ai đó đã gộp code mới vào `main`, hãy kéo code mới đó về nhánh của bạn để tránh conflict sau này:
```bash
git checkout main
git pull origin main
git checkout feature/<tên-task-của-bạn>
git merge main
```
*Lưu ý: Nếu có conflict (đụng độ code), hãy bình tĩnh mở file bị conflict lên, xem xét phần code nào nên giữ, phần nào bỏ, lưu lại và commit.*

---
Chúc cả nhóm hoàn thành tốt dự án!
