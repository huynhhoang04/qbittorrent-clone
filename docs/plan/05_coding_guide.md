# Hướng dẫn Tư duy Code - BitTorrent Clone

Tài liệu này cung cấp hướng dẫn tư duy chi tiết về cách thiết kế và lập trình từng thành phần cho dự án BitTorrent Clone bằng C++17. Mục tiêu là giúp các developer (đặc biệt là sinh viên) hiểu được **CÁCH NGHĨ (HOW TO THINK)** khi implement từng tính năng, thay vì chỉ biết **CODE GÌ (WHAT TO IMPLEMENT)**.

## 1. Tư duy tổng quan: Nghĩ theo Layer

Khi xây dựng một hệ thống phân tán phức tạp như BitTorrent, hãy tư duy theo các tầng (layers). Việc chia nhỏ thành các tầng giúp bạn kiểm thử độc lập và xây dựng từ dưới lên một cách chắc chắn:

*   **Layer 1: Data Layer**: Xử lý I/O tệp, chia nhỏ file (chunking), băm SHA-256 (hashing), và lưu trữ trạng thái (SQLite).
*   **Layer 2: Protocol Layer**: Tuần tự hóa/giải tuần tự hóa thông điệp (message serialization), định nghĩa giao thức trao đổi dữ liệu (wire protocol).
*   **Layer 3: Network Layer**: Xử lý kết nối mạng qua TCP sockets, gọi API HTTP (HTTP client cho tracker).
*   **Layer 4: Logic Layer**: Trái tim của ứng dụng: quản lý luồng tải xuống (download manager), quản lý trạng thái (state manager), điều phối peer (peer orchestration).
*   **Layer 5: UI Layer**: Giao diện người dùng dạng TUI sử dụng ncurses.

**Nguyên tắc**: Xây dựng từ dưới lên (Bottom-up). Bắt đầu với Layer 1, viết Unit Test cho nó, đảm bảo nó chạy đúng trước khi chuyển sang Layer 2.

---

## 2. Cách tư duy từng Component

### 2.1. SHA-256 Wrapper
*   **Tư duy**: Hãy coi SHA-256 như một pure function: đầu vào là bytes -> đầu ra là chuỗi hash.
*   **Thực thi**: Sử dụng API `EVP` của OpenSSL (không dùng hàm `SHA256()` cũ đã bị deprecated).
*   **2 Chế độ**: Hash một buffer trong RAM và Hash một file từ đĩa.
*   **Bí quyết cốt lõi**: **KHÔNG BAO GIỜ** load toàn bộ một file lớn vào RAM. Sử dụng `EVP_DigestUpdate` trong một vòng lặp đọc file theo từng khối nhỏ (streaming).

### 2.2. Metadata Generator
*   **Tư duy**: "Tôi cần mô tả một file một cách hoàn chỉnh nhất để người khác có thể xác minh từng mảnh nhỏ của nó."
*   **Thuật toán**: Mở file gốc -> đọc `chunk_size` bytes mỗi lần -> hash chunk đó -> (cuối cùng) hash toàn bộ file -> lưu tất cả thông tin (tên file, kích thước, mảng các hash, tracker URL) dưới dạng JSON.
*   File `.torrent.json` chính là "bản hợp đồng" giữa seeder và leecher.

### 2.3. File Manager
*   **Tư duy**: "Dữ liệu sẽ chảy từ mạng -> đĩa cứng -> file hoàn chỉnh như thế nào?"
*   **Quyết định thiết kế**: Có 2 cách:
    1.  Tạo trước một file to (pre-allocate) và ghi đè vào các offset. (Hiệu năng tốt nhưng khó quản lý và debug).
    2.  Lưu từng chunk thành các file riêng biệt (VD: `chunks/chunk_000`, `chunk_001`). (Đơn giản hơn, cực dễ debug).
*   **Lựa chọn cho dự án này**: Sử dụng các file chunk riêng biệt.
*   **Quá trình Merge**: Khi tải xong, chỉ cần đọc từng file chunk theo thứ tự, ghi vào file đích, và xác minh lại hash cuối cùng.

### 2.4. Wire Protocol
*   **Tư duy**: "Hai tiến trình đang nói chuyện qua một luồng TCP thô. Làm sao chúng biết được khi nào một thông điệp kết thúc và thông điệp mới bắt đầu?"
*   **Giải pháp**: Sử dụng Length-prefixed messages (Thông điệp có độ dài ở đầu).
    *   4 bytes đầu = độ dài thông điệp.
    *   Byte tiếp theo = Message ID.
    *   Còn lại = Payload.
*   **QUAN TRỌNG (CRITICAL)**: TCP là một **STREAM (dòng chảy)**, không phải message-based. Bạn có thể nhận được nửa thông điệp, hoặc 2 thông điệp dính liền nhau trong 1 lần `recv()`.
*   **Hành động bắt buộc**:
    *   Viết hàm `recv_exact(socket, n_bytes)`: sử dụng vòng lặp until đọc đủ `n` bytes.
    *   Viết hàm `send_exact(socket, buffer, n_bytes)`: sử dụng vòng lặp until gửi hết `n` bytes.
*   **Network Byte Order**: TCP network sử dụng big-endian. Bắt buộc dùng `htonl`/`ntohl` cho tất cả các số nguyên nhiều bytes.

### 2.5. SQLite Database
*   **Tư duy**: "Nếu chương trình bị crash giữa chừng, trạng thái nào cần được phục hồi?"
*   **Dữ liệu cần lưu**: Danh sách torrent, trạng thái tải từng chunk, lịch sử kết nối peer.
*   **Tối ưu hóa**: Kích hoạt chế độ WAL để cho phép đọc/ghi đồng thời: `PRAGMA journal_mode=WAL;`.
*   **Thread Safety**: Bao bọc mọi thao tác DB trong class `Database` với một `std::mutex`.
*   **An toàn & Hiệu năng**: Luôn sử dụng *prepared statements* (tránh SQL injection và chạy nhanh hơn).
*   **Quy luật vàng**: Chỉ cập nhật trạng thái chunk thành 'verified' trong DB **SAU KHI** kiểm tra SHA-256 thành công.

### 2.6. Tracker
*   **Tư duy**: "Tôi là cuốn danh bạ điện thoại. Các Peer báo cho tôi biết họ đang tồn tại, và họ cũng hỏi tôi xem có ai khác đang tồn tại không."
*   **Thiết kế**: Rất đơn giản, chỉ là một `Map<file_id, List<Peer_Info>>`.
*   **Độ phức tạp**: Nằm ở Thread Safety, vì nhiều peer có thể gọi API cùng lúc.
*   **Thực thi**: Sử dụng tính năng đa luồng tích hợp của thư viện Crow HTTP, nhưng bảo vệ Map dữ liệu bằng `std::mutex`. Cần một background thread chạy ngầm mỗi 10 giây để xóa các peer "chết" (VD: `last_seen` > 45 giây).

### 2.7. Upload Server (TCP Server on Peer)
*   **Tư duy**: "Tôi ngồi chờ ở cửa. Khi có ai kết nối, tôi sẽ lấy chunk từ đĩa và đưa cho họ."
*   **Mô hình**: Vòng lặp `socket()` -> `bind()` -> `listen()` -> `accept()`.
*   **Xử lý đa luồng**: Với mỗi kết nối được `accept`, sinh ra một thread mới để phục vụ nó.
*   **Luồng xử lý (Handler thread)**: Nhận `HANDSHAKE` -> Gửi `BITFIELD` -> Chờ lệnh `REQUEST` -> Đọc chunk từ đĩa cứng -> Gửi tin nhắn `PIECE` -> Lặp lại chờ lệnh.
*   **Insight**: Các kết nối này độc lập hoàn toàn với nhau. Chúng không chia sẻ state gì ngoài việc cùng đọc dữ liệu file (read-only), do đó không lo bị race condition khi đọc file.

### 2.8. Download Manager
*   **Tư duy**: "Tôi có một danh sách đi chợ (các chunks cần tải). Tôi có nhiều siêu thị (các peers) để mua. Làm sao tôi đi chợ nhanh nhất?"
*   **Cấu trúc dữ liệu**:
    *   `missing_chunks`: Queue an toàn luồng (Thread-safe queue) chứa index các chunk cần tải.
    *   `in_flight`: Set (tập hợp) các chunk ĐANG được tải để tránh tải trùng.
    *   `peer_list`: Danh sách các siêu thị hiện có.
*   **Mô hình Worker Thread**:
    1.  Pop một `chunk_index` từ `missing_chunks`.
    2.  Chọn một peer sở hữu chunk này (dựa vào `bitfield` của peer đó).
    3.  Kết nối tới peer, handshake, request chunk.
    4.  Nhận dữ liệu chunk.
    5.  Hash dữ liệu nhận được và so sánh với hash trong metadata.
    6.  Nếu ĐÚNG: Ghi ra đĩa, báo lên SQLite, cập nhật `bitfield`.
    7.  Nếu SAI: Ném `chunk_index` quay lại hàng đợi, thử tải lại từ peer khác.
    8.  Lặp lại cho đến khi queue rỗng.

### 2.9. State Manager
*   **Tư duy**: "Tôi là Nguồn Sự Thật Duy Nhất (Single Source of Truth) về việc Peer này đang có những gì."
*   **Biểu diễn dữ liệu**: Sử dụng `std::vector<bool>` hoặc `std::vector<uint8_t>` làm Bitfield đại diện cho danh sách chunk.
*   **Khởi động**: Đọc trạng thái từ SQLite -> Xây dựng Bitfield.
*   **Cập nhật**: Khi một chunk được xác minh thành công -> cập nhật Bitfield, cập nhật SQLite.
*   **Thread Safety**: Class này phải tuyệt đối an toàn luồng vì sẽ có nhiều Download Worker thread gọi hàm cập nhật cùng một lúc.

### 2.10. Peer Node (Orchestrator)
*   **Tư duy**: "Tôi là nhạc trưởng. Tôi không tự chơi nhạc cụ nào, nhưng tôi điều phối tất cả mọi người."
*   **Lệnh Seed**: Sinh metadata -> Lưu vào SQLite -> Chạy Upload Server -> Bắt đầu thread gửi Announce lên Tracker.
*   **Lệnh Download**: Tải metadata -> Lưu vào SQLite -> Hỏi Tracker tìm Peer -> Chạy Upload Server -> Chạy Download Manager -> Bắt đầu thread gửi Announce.
*   **Quản lý vòng đời**: Khởi động các component, theo dõi tiến độ, và đảm bảo mọi thứ tắt (shutdown) một cách an toàn, không thất thoát tài nguyên.

### 2.11. TUI (ncurses)
*   **Tư duy**: "Tôi là một người vẽ tranh đứng ở một góc riêng, thỉnh thoảng nhìn vào trạng thái của hệ thống và vẽ lại."
*   **Nguyên tắc tuyệt đối**: TUI **KHÔNG BAO GIỜ** được làm block các logic chính của mạng hay file.
*   **Thực thi**: Chạy TUI trên một Thread riêng, sử dụng vòng lặp định kỳ (VD: mỗi 100ms) đọc trạng thái từ `StateManager`/`Database` để vẽ lại màn hình.
*   **Hàm ncurses**: Khởi tạo `initscr()`, chia panel bằng `newwin()`, in chữ bằng `wprintw()`, cập nhật màn hình `wrefresh()`, và quan trọng nhất: cấu hình `getch()` thành non-blocking bằng `nodelay()`.

---

## 3. Các Design Pattern quan trọng

### 3.1. Producer-Consumer (Dành cho Download Manager)
*   **Producer**: Việc truy vấn Tracker sinh ra danh sách peer, từ đó nạp công việc vào queue `missing_chunks`.
*   **Consumer**: Các luồng Download Worker "ăn" công việc từ queue, tải, và xác minh.
*   Sử dụng `std::condition_variable` để "đánh thức" các worker đang ngủ khi có công việc mới.

### 3.2. Observer Pattern (Cập nhật trạng thái)
*   Khi một chunk được tải và xác minh xong, nhiều nơi cần biết điều này:
    *   TUI (cần cập nhật thanh tiến trình).
    *   StateManager (cần cập nhật Bitfield nội bộ).
    *   Database (cần lưu trạng thái vĩnh viễn).
    *   Tracker Client (có thể cần báo cáo tiến độ mới).

### 3.3. RAII cho Quản lý Tài nguyên (Resource Acquisition Is Initialization)
*   **Sockets**: Gói `int sockfd` trong một class C++, và bắt buộc gọi `close(sockfd)` trong Destructor `~Socket()`.
*   **SQLite**: Wrap connection và gọi `sqlite3_close` ở Destructor.
*   **Files**: Ưu tiên sử dụng `std::ifstream` / `std::ofstream` vì chúng tự động đóng khi ra khỏi scope.
*   **Mutexes**: **KHÔNG BAO GIỜ** dùng `mutex.lock()` hay `mutex.unlock()` bằng tay. Luôn dùng `std::lock_guard` hoặc `std::unique_lock`.

---

## 4. Những cái bẫy thường gặp (Common Pitfalls)

### 4.1. TCP Partial Reads/Writes (Kẻ giết người thầm lặng)
Như đã nhắc ở trên, `recv()` có quyền trả về ít số byte hơn bạn yêu cầu, và `send()` cũng vậy. Cần vòng lặp until để đảm bảo tính toàn vẹn.

### 4.2. Endianness (Lệch kiến trúc)
Mạng luôn dùng **Big-endian**, nhưng máy tính x86 (Windows, Mac Intel, Linux) luôn dùng **Little-endian**. Mọi biến `int32_t` hay `int16_t` khi nhét vào socket bắt buộc phải qua `htonl() / htons()`.

### 4.3. Deadlocks với Thread Safety
*   Tránh việc giữ 2 mutex cùng lúc.
*   Nếu bắt buộc phải lấy 2 mutex, thì ở TẤT CẢ mọi nơi trong code, thứ tự khóa phải **TUYỆT ĐỐI GIỐNG NHAU**.

### 4.4. Rò rỉ File Descriptor (File Descriptor Leaks)
Hệ điều hành giới hạn số file/socket mở cùng lúc (VD: 1024). Nếu bạn kết nối tải xong mà quên gọi `close()`, app sẽ sớm bị sập với lỗi `Too many open files`. RAII là cách giải quyết.

### 4.5. Signal Handling (Nhấn Ctrl+C)
Khi người dùng bấm `Ctrl+C`, bạn không thể để chương trình chết đứng.
*   Dùng thư viện `<csignal>` để bắt `SIGINT`.
*   Set một biến cờ `std::atomic<bool> is_running = false;`.
*   Các vòng lặp while trong chương trình phải check biến này để tự thoát.
*   Đảm bảo dọn dẹp SQLite (flush) và gọi `endwin()` cho ncurses để trả lại terminal bình thường cho user.

---

## 5. Quy trình Debug

1.  **Đơn giản nhất**: Test cục bộ với 1 Seeder và 1 Leecher trên cùng 1 máy (khác port).
2.  **Ghi Log (Logging)**: Trong giai đoạn đầu, hãy in log MỌI THỨ ra file (vì màn hình terminal đang bị ncurses chiếm dụng).
3.  **Bắt gói tin**: Dùng Wireshark (hoặc TCPDump) để soi xem các gói dữ liệu Handshake, Request, Piece bay qua TCP có đúng format không.
4.  **Kiểm tra DB**: Dùng CLI của `sqlite3` (`sqlite3 my.db`) để query trực tiếp xem dữ liệu đang được lưu có chuẩn không.
5.  **Nguyên tắc**: Test độc lập Layer bằng Unit Test TRƯỚC KHI tích hợp thành luồng dài.

---

## 6. Thứ tự code khuyến nghị (Roadmap)

Đừng cố viết tất cả trong 1 ngày, hãy làm theo lộ trình này:

1.  Viết `sha256` wrapper + Unit test.
2.  Viết `metadata` generator + Unit test.
3.  Viết `file_manager` (chia chunk / nối chunk) + Unit test.
4.  Định nghĩa `protocol` (hàm serialize/deserialize header/message) + Unit test.
5.  Viết `database` wrapper SQLite + Unit test.
6.  Viết `tracker` (Crow HTTP Server) + Dùng Postman hoặc `curl` để test thủ công API.
7.  Viết `upload_server` (TCP): chỉ cần nhận 1 kết nối, đọc 1 chunk, nhả về 1 khối dữ liệu tĩnh + Test bằng script Python nhỏ.
8.  Viết `download_manager`: 1 thread, tải từ 1 peer, đúng 1 chunk rồi lưu ra file.
9.  **Tích hợp lần 1**: Chạy quy trình Tải một file trọn vẹn từ 1 Peer sang 1 Peer khác, với 1 worker thread.
10. Nâng cấp: Nhiều Download worker threads (Concurrency).
11. Nâng cấp: Tải từ nhiều Peers khác nhau.
12. Thêm tính năng lưu trạng thái để tắt đi bật lại tải tiếp (Resume capability).
13. Tích hợp `ncurses` TUI để hiển thị cho đẹp.
14. Hoàn thiện, xử lý edge-cases, bắt lỗi ngắt mạng, v.v.
