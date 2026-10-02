# Các Vấn Đề Về Hệ Thống Phân Tán (Distributed System Concerns)

Tài liệu này trình bày phân tích chi tiết về các vấn đề của hệ thống phân tán trong ngữ cảnh của dự án P2P File-Sharing System (BitTorrent Clone - DS02). Dự án sử dụng C++17, POSIX Sockets, Crow HTTP, OpenSSL, SQLite3, và ncurses.

Mỗi vấn đề được phân tích theo cấu trúc:
1. **Vấn đề là gì** (Khái niệm chung)
2. **Biểu hiện trong hệ thống của chúng ta** (Ngữ cảnh cụ thể)
3. **Giải pháp cụ thể** (Cách hệ thống xử lý)

---

## 1. Định danh và Khám phá (Naming & Discovery)

### Vấn đề là gì?
Trong một hệ thống phân tán, các thực thể (node, file, tài nguyên) cần có một định danh duy nhất (Naming) trên toàn bộ mạng lưới. Đồng thời, các node cần cách thức để tìm thấy nhau và tìm thấy tài nguyên chúng cần (Discovery) mà không cần cấu hình tĩnh.

### Biểu hiện trong hệ thống
- Làm thế nào một Peer phân biệt được nó với các Peer khác?
- Làm thế nào để định danh một tệp (file) một cách duy nhất mà không phụ thuộc vào tên tệp?
- Làm sao để một Peer mới tham gia mạng lưới biết được ai đang giữ các mảnh (chunk) của tệp nó muốn tải?

### Giải pháp cụ thể
- **Peer ID**: Sử dụng UUID v4 làm định danh duy nhất cho mỗi peer. ID này được tạo ra trong lần chạy đầu tiên và được lưu trữ vĩnh viễn (persisted) trong tệp `peer_config.json`.
- **File ID (InfoHash)**: Định danh tệp bằng mã băm SHA-256 của toàn bộ nội dung tệp. Điều này đảm bảo tính duy nhất và tính toàn vẹn của tệp dựa trên nội dung.
- **Tracker (Discovery Service)**: Tracker đóng vai trò là dịch vụ khám phá tập trung. 
- **Quy trình khám phá**: 
  - Peer kết nối và báo cáo (announce) với Tracker về các tệp nó đang giữ.
  - Khi một Peer muốn tải tệp, nó gửi request lên Tracker với File ID.
  - Tracker trả về danh sách các Peer ID kèm theo IP/Port đang sở hữu tệp đó.

---

## 2. Đồng thời và Đồng bộ hóa (Concurrency & Synchronization)

### Vấn đề là gì?
Nhiều tiến trình hoặc luồng chạy song song cần truy cập và sửa đổi các tài nguyên dùng chung, dẫn đến rủi ro về race condition, deadlock và mất dữ liệu nếu không được đồng bộ hóa đúng cách.

### Biểu hiện trong hệ thống
- Nhiều Peer cùng yêu cầu (request) tải các chunk từ một Peer đồng thời.
- Một Peer vừa đóng vai trò uploader (gửi chunk) vừa là downloader (nhận chunk) tại cùng một thời điểm.
- Trạng thái tệp, bitfield, và cơ sở dữ liệu SQLite bị truy cập đồng thời bởi nhiều luồng (thread pool tải về, luồng xử lý kết nối đến).

### Giải pháp cụ thể
- **Thread Safety**: Sử dụng `std::mutex`, `std::shared_mutex` (đọc/ghi) để bảo vệ các cấu trúc dữ liệu dùng chung (ví dụ: danh sách kết nối, trạng thái tải, bitfield).
- **Chunk Selection Strategy**: Áp dụng chiến lược "Random-first" (cho chunk đầu tiên để nhanh chóng có dữ liệu trao đổi) và "Rarest-first" (cho các chunk sau để tối ưu hóa sự phân tán dữ liệu hiếm).
- **Giới hạn kết nối (Connection Limits)**: Áp dụng giới hạn tối đa `N` luồng tải về (download threads) và tối đa `M` kết nối tải lên (upload connections) để tránh quá tải tài nguyên mạng và CPU.
- **Race Condition Handling**: Xử lý trùng lặp trong hàng đợi tải (download queue deduplication). Đảm bảo một chunk không bị nhiều luồng cùng yêu cầu tải về từ các Peer khác nhau.
- **SQLite Thread Safety**: Sử dụng chế độ **WAL (Write-Ahead Logging)** cho SQLite kết hợp với mutex ở lớp ứng dụng (app-level DB access mutex) để cho phép đọc ghi đồng thời với hiệu suất cao mà không gây lock database kéo dài.

---

## 3. Tính nhất quán (Consistency)

### Vấn đề là gì?
Trong mạng P2P, trạng thái cục bộ của mỗi node không thể cập nhật tức thời trên toàn bộ mạng. Sự thống nhất thông tin (như ai có chunk nào) giữa các node là một thách thức.

### Biểu hiện trong hệ thống
- Tracker và các Peer khác không biết ngay lập tức khi một Peer vừa tải xong một chunk mới.
- Dữ liệu nhận được có thể bị lỗi, dẫn đến trạng thái tải không nhất quán với dữ liệu thực tế.

### Giải pháp cụ thể
- **Eventual Consistency**: Chấp nhận tính nhất quán cuối cùng cho bitfield giữa các Peer. Thông tin về bitfield chỉ lan truyền qua các thông điệp định kỳ (Have message) hoặc chu kỳ báo cáo (announce).
- **Cập nhật Bitfield**: Khi một chunk tải về hoàn tất và được xác minh, Peer sẽ cập nhật trạng thái ngay lập tức vào cơ sở dữ liệu SQLite, sau đó báo cáo lại trạng thái mới nhất lên Tracker.
- **Tính toàn vẹn của Chunk (Chunk Integrity)**: Mỗi chunk được xác thực ngay sau khi nhận bằng mã băm SHA-256 (kiểm tra với danh sách mã băm gốc).
- **Tính toàn vẹn của Tệp (File Integrity)**: Sau khi ráp toàn bộ tệp từ các chunk, thực hiện băm SHA-256 toàn bộ file một lần nữa để đối chiếu với File ID ban đầu.
- **Hash Failure**: Nếu xác minh chunk thất bại, chunk bị loại bỏ, đánh dấu là 'missing' (thiếu) trong SQLite DB, và được đưa lại vào hàng đợi để yêu cầu từ một Peer khác.

---

## 4. Chịu lỗi và Phục hồi (Fault Tolerance & Recovery)

### Vấn đề là gì?
Các hệ thống phân tán thường xuyên gặp sự cố: mạng rớt, node sập, gói tin hỏng, dịch vụ trung tâm (tracker) ngưng hoạt động. Hệ thống phải tiếp tục chạy hoặc có khả năng phục hồi khi có lỗi.

### Kịch bản 1: Peer rời đi giữa chừng khi đang tải (Churn)
- **Biểu hiện**: Peer đang gửi dữ liệu đột ngột ngắt kết nối (TCP RST hoặc Timeout).
- **Giải pháp**: 
  - *Phát hiện*: Lỗi socket TCP (Connection reset) hoặc timeout chờ dữ liệu vượt quá 5s.
  - *Phản hồi*: Đưa chunk đang tải dở dang trở lại hàng đợi (queue), và chọn một Peer khác từ danh sách đã biết để tải lại chunk đó.
  - *Kế hoạch dự phòng*: Nếu không còn Peer nào giữ chunk đó, tạm dừng và truy vấn lại Tracker định kỳ để tìm Peer mới.

### Kịch bản 2: Tracker gặp sự cố (Tracker goes down)
- **Biểu hiện**: Không thể phân giải HTTP Request đến Tracker.
- **Giải pháp**:
  - Các Peer đã kết nối và đang trao đổi dữ liệu sẽ **tiếp tục** quá trình tải/tải lên dựa trên danh sách Peer hiện tại.
  - Các Peer mới tham gia sẽ không thể tìm thấy tệp cho đến khi Tracker hoạt động lại.
  - *Caching*: Các Peer sử dụng bảng `peer_history` trong SQLite để lưu trữ danh sách Peer đã từng biết. Khi Tracker sập, Peer có thể thử kết nối với các Peer trong lịch sử để hỏi dữ liệu.

### Kịch bản 3: Dữ liệu bị hỏng (Corrupted data)
- **Biểu hiện**: Bị can thiệp trên đường truyền hoặc đọc hỏng trên đĩa, mã băm không khớp.
- **Giải pháp**:
  - *Phát hiện*: Kiểm tra SHA-256 thất bại.
  - *Phản hồi*: Loại bỏ dữ liệu hỏng, tạm thời đưa địa chỉ Peer gửi vào danh sách đen (blacklist) trong phiên làm việc, và yêu cầu lại chunk từ Peer khác.
  - *Ghi log*: Ghi lại sự kiện tham nhũng dữ liệu để phục vụ kiểm tra (audit) sau này.

### Kịch bản 4: Peer khởi động lại (Recovery)
- **Biểu hiện**: Peer bị crash, tắt máy hoặc mất điện, sau đó bật lại.
- **Giải pháp**:
  - Khi khởi động lại, Peer đọc dữ liệu từ SQLite DB để khôi phục toàn bộ trạng thái.
  - Bảng `torrents` cho biết những tệp nào đang được quản lý.
  - Bảng `chunks` xác định chính xác chunk nào đã có (verified) và chunk nào còn thiếu (missing).
  - Bảng `peer_history` cung cấp danh sách Peer 캐 cache.
  - Peer tự tự động tiếp tục (resume) quá trình tải các chunk còn thiếu mà không cần tải lại từ đầu.

### Các Cơ chế Phục hồi Chung
- **Timeout**: Áp dụng cho các socket TCP và các lệnh báo cáo lên Tracker.
- **Bounded Retry**: Số lần thử lại tối đa (max 3 lần) cho một chunk từ một Peer để tránh vòng lặp vô hạn.
- **Heartbeat**: Định kỳ gửi thông báo "announce" lên Tracker để báo rằng Peer còn sống.
- **Checkpointing**: Sử dụng SQLite để lưu trạng thái kiên định. Trạng thái của chunk được lưu vĩnh viễn (persisted) vào DB ngay sau khi chunk được xác minh, đóng vai trò như điểm checkpoint.
- **Reconnection**: Cơ chế tự động thử kết nối lại hoặc truy vấn lại Tracker sau khoảng thời gian ngẫu nhiên.

---

## 5. Bảo mật cơ bản (Basic Security)

### Vấn đề là gì?
Sự tham gia của các node không tin cậy đòi hỏi phải xác thực nguồn gốc và nội dung dữ liệu, cũng như chống lại các cuộc tấn công cơ bản (như DoS, gửi dữ liệu quá lớn).

### Biểu hiện trong hệ thống
- Các Peer độc hại có thể gửi dữ liệu giả mạo.
- Cố gắng mở quá nhiều kết nối làm cạn kiệt tài nguyên node (Connection exhaustion).
- Gửi thông điệp không đúng chuẩn gây crash ứng dụng.

### Giải pháp cụ thể
- **Xác thực Peer (Peer Authentication)**: Thực hiện qua quá trình handshake chuẩn (Peer ID và InfoHash phải khớp).
- **Tính Toàn Vẹn (Data Integrity)**: SHA-256 chặn hoàn toàn dữ liệu bị làm giả.
- **Kiểm tra Đầu vào (Input Validation)**: Định dạng tin nhắn (message protocol), giới hạn chỉ số index (chunk index bounds), và kiểm tra kích thước payload chặt chẽ trước khi xử lý.
- **Rate Limiting**: Hạn chế băng thông hoặc số lượng yêu cầu (request) trên mỗi kết nối TCP.
- **Max Message Size**: Định nghĩa kích thước tin nhắn tối đa (ví dụ 16KB hoặc 32KB cho một chunk data) để chống tràn bộ đệm (Buffer overflow/OOM).
- **Logging**: Ghi log đầy đủ các sự kiện (kết nối, lỗi, chặn IP) để phục vụ debug và bảo mật.
- **Cấu hình (Config)**: Các tham số nhạy cảm được cấu hình qua tệp `.env`, không hardcode vào mã nguồn.

---

## 6. Xử lý tin nhắn trùng lặp và không đúng thứ tự

### Vấn đề là gì?
Trong mạng phân tán, tin nhắn có thể đến chậm, đến sớm, đến trùng lặp hoặc bị nhận nhiều lần (do mạng cấu hình sai hoặc logic retry).

### Biểu hiện trong hệ thống
- Nhận được cùng một khối dữ liệu chunk từ hai Peer khác nhau (ví dụ: do timeout quá ngắn dẫn đến yêu cầu lại trước khi nhận được).
- Gói tin đến không đúng thứ tự từ các Peer.

### Giải pháp cụ thể
- **Chunk Idempotency**: Việc ghi một chunk vào tệp hoặc DB là hành động lũy đẳng (idempotent). Ghi lại một chunk đúng nội dung nhiều lần không làm thay đổi trạng thái tổng thể.
- **Chỉ số Chunk (Chunk Index)**: Việc có chỉ số index rõ ràng cho từng chunk (offset) đảm bảo việc ráp nối cuối cùng (merge) luôn đúng thứ tự, bất kể thứ tự tải về.
- **Deduplication**: Quản lý các yêu cầu (REQUEST) đang chờ phản hồi (in-flight tracking). Nếu chunk đang in-flight, không gửi REQUEST cho peer khác trừ khi quá thời gian timeout.
- **Đảm bảo của TCP**: TCP đảm bảo việc truyền tải không mất gói và đúng thứ tự *trong phạm vi một kết nối* giữa hai Peer.

---

## 7. Hiệu năng và Khả năng mở rộng (Performance & Scalability)

### Vấn đề là gì?
Hệ thống cần tải dữ liệu càng nhanh càng tốt, tận dụng tối đa băng thông, trong khi không làm quá tải CPU hay memory.

### Biểu hiện trong hệ thống
- Tải tuần tự quá chậm.
- Disk I/O trở thành nút thắt cổ chai (bottleneck).
- Tracker bị quá tải nếu số lượng kết nối quá lớn.

### Giải pháp cụ thể
- **Parallel Downloads**: Sử dụng Thread Pool (luồng công nhân) để tải đa luồng cùng lúc từ nhiều Peer.
- **Load Balancing**: Phân phối tải (Load balancing) ở cấp độ chunk. Việc lựa chọn Peer linh hoạt giúp cân bằng băng thông giữa các seeder trong mạng.
- **Tracker Tối ưu**: Sử dụng bộ nhớ trong (in-memory) như `std::unordered_map` cho Tracker để tra cứu (lookup) Peer O(1).
- **Tối ưu Disk I/O với SQLite**: Sử dụng chế độ WAL trong SQLite cho phép các luồng tải đọc và ghi trạng thái đồng thời với hiệu năng cực cao.
- **Bottleneck Analysis & Phân tích Metrics**: 
  - Thu thập các chỉ số về tốc độ tải, độ trễ từng chunk (latency), mức sử dụng CPU/RAM để tối ưu hóa.
  - Sử dụng Profiling tool định kỳ.

---

## Tổng kết (Summary Table)

| Vấn đề Hệ thống Phân tán (Concern) | Kỹ thuật / Giải pháp Áp dụng (Solution) |
| :--- | :--- |
| **Định danh (Naming)** | UUID v4 (Peer ID), SHA-256 (File InfoHash). |
| **Khám phá (Discovery)** | Centralized Tracker thông qua HTTP API, lưu trữ cục bộ danh sách trong DB. |
| **Đồng thời (Concurrency)** | Mutexes, Thread Pools, SQLite WAL, Giới hạn kết nối (N download/M upload). |
| **Nhất quán (Consistency)** | SHA-256 verification cho từng chunk, Eventual consistency cho bitfield lan truyền. |
| **Chịu lỗi & Phục hồi (Fault/Recovery)** | Timeout 5s, Retry 3 lần, Heartbeat tới Tracker, SQLite checkpointing, Tự động resume. |
| **Bảo mật (Security)** | Protocol handshake, Message/Payload validation, Rate limiting. |
| **Trùng lặp & Không thứ tự (Ordering)** | Tính lũy đẳng (Idempotent chunks), Chunk Indexing, TCP guarantees, In-flight tracker. |
| **Hiệu suất & Khả năng mở rộng (Scaling)**| Rarest-first, Random-first, Thread pool song song, O(1) Tracker In-memory, SQLite WAL. |
