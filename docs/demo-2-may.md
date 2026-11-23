# Demo DS02 — 2 người, 2 máy

**A (Huynh):** chạy tracker và nguồn S. **B (Bách):** chạy peer P và peer L.
Lệnh dùng Linux/Bash, chạy từ thư mục gốc repo; hai máy đã có `build-release`.
Thay **192.168.1.10** bằng IP máy A, **192.168.1.20** bằng IP máy B.
Dùng `/tmp/ds02` mới; nếu đã diễn tập, thay nó bằng thư mục khác trong toàn bộ lệnh.

| Node | Máy | Cổng P2P | Dashboard mở trên máy đó |
| --- | --- | --- | --- |
| Tracker | A | HTTP 18080 | — |
| S | A | 16881 | http://localhost:18081 |
| P | B | 16882 | http://localhost:18082 |
| L | B | 16883 | http://localhost:18083 |

## 1. Khởi động — mỗi lệnh chạy node ở một terminal riêng

**A, terminal 1 — bật tracker để các peer tìm nhau:**

```bash
./build-release/src/tracker/TrackerApp -p 18080
```

**A, terminal 2 — tạo tệp 128 MiB và chia sẻ từ S:**

```bash
mkdir -p /tmp/ds02
head -c 134217728 /dev/urandom > /tmp/ds02/demo.bin
./build-release/src/peer/PeerApp --tracker http://192.168.1.10:18080 \
  --announce-ip 192.168.1.10 -p 16881 --data-dir /tmp/ds02/s1 \
  --seed /tmp/ds02/demo.bin --chunk-size 262144 --max-uploads 1 \
  --chunk-delay-ms 100 --webui --webui-port 18081
```

**B, terminal 1 — bật P để tải và phục vụ lại mảnh:**

```bash
./build-release/src/peer/PeerApp --tracker http://192.168.1.10:18080 \
  --announce-ip 192.168.1.20 -p 16882 --data-dir /tmp/ds02/p1 \
  --download-limit-mbps 10 --max-downloads 1 --max-uploads 4 --chunk-delay-ms 150 \
  --webui --webui-port 18082
```

**B, terminal 2 — bật L để nhận mảnh:**

```bash
./build-release/src/peer/PeerApp --tracker http://192.168.1.10:18080 \
  --announce-ip 192.168.1.20 -p 16883 --data-dir /tmp/ds02/l1 \
  --download-limit-mbps 10 --max-downloads 2 --pipeline-depth 1 --io-timeout-ms 6000 \
  --webui --webui-port 18083
```

Mỗi peer tải giới hạn 10 MiB/s bằng `--download-limit-mbps 10` (tổng từ mọi nguồn).
Delay giúp tải đủ chậm để quan sát; nó có thể làm tốc độ thấp hơn mức giới hạn. S có **1 slot** để ưu tiên phục vụ P; L sẽ nhận mảnh từ P.

## 2. Chứng minh peer chưa tải xong đã upload được

| Ai | Thao tác | Để làm gì / nhìn gì |
| --- | --- | --- |
| A | Mở dashboard S → bấm **`.torrent`** ở dòng `demo.bin`; chuyển **file JSON này** sang B. | Cho B metadata chứa hash và thông tin tracker; không chuyển tệp dữ liệu 128 MiB. |
| B | Mở dashboard P → **Add Torrent** → chọn JSON → để **Output Directory trống** → **Start Download**. | P bắt đầu lấy dữ liệu từ S; file sẽ nằm ở `/tmp/ds02/p1/demo.bin`. |
| B | Khi P khoảng **10–20%**, làm tương tự trên dashboard L. | Cho L nhận mảnh trong lúc P vẫn đang tải. |
| B | Trên P, chọn dòng tệp → mở tab **Peers**; nhìn **Progress** và **Total Uploaded**. | **Đạt:** P chưa 100% nhưng Total Uploaded > 0; có kết nối upload tới L. |
| B | Trên L, mở **Peers**, đối chiếu Peer ID/endpoint của P. | **Đạt:** P là nguồn đã gửi mảnh cho L; bộ đếm Chunks tăng. |
| B | Chụp màn hình kết quả; **Ctrl+C terminal L**, để P tải tới **100%**. | Giữ P làm nguồn đầy đủ cho bước tiếp theo. |

Nếu P đã 100% trước lúc quan sát upload, bước này chưa chứng minh được yêu cầu; làm lại với L bắt đầu sớm hơn.

## 3. Tải từ hai nguồn, dừng một nguồn, kiểm tra hash

**Chuẩn bị:** khi P đã 100%, B Ctrl+C terminal P. A Ctrl+C terminal S và tracker.
A bật lại tracker bằng lệnh ở mục 1 để xóa danh sách peer cũ.

**A bật S, lần này cho phép nhiều kết nối:**

```bash
./build-release/src/peer/PeerApp --tracker http://192.168.1.10:18080 \
  --announce-ip 192.168.1.10 -p 16881 --data-dir /tmp/ds02/s2 \
  --seed /tmp/ds02/demo.bin --max-uploads 4 --chunk-delay-ms 200 \
  --webui --webui-port 18081
```

**B bật P thành nguồn đầy đủ, dùng tệp vừa tải xong:**

```bash
./build-release/src/peer/PeerApp --tracker http://192.168.1.10:18080 \
  --announce-ip 192.168.1.20 -p 16882 --data-dir /tmp/ds02/p2 \
  --seed /tmp/ds02/p1/demo.bin --max-uploads 4 --chunk-delay-ms 200 \
  --webui --webui-port 18082
```

**B bật L với thư mục mới để tải từ đầu:**

```bash
./build-release/src/peer/PeerApp --tracker http://192.168.1.10:18080 \
  --announce-ip 192.168.1.20 -p 16883 --data-dir /tmp/ds02/l2 \
  --download-limit-mbps 10 --max-downloads 2 --pipeline-depth 1 --io-timeout-ms 6000 \
  --webui --webui-port 18083
```

| Ai | Thao tác | Để làm gì / nhìn gì |
| --- | --- | --- |
| B | Xác nhận S và P đều **100%/Seeding**; trên L **Add Torrent** lại JSON, để Output Directory trống. | Bắt đầu tải với hai nguồn đầy đủ. |
| B | Trên L chọn tệp → **Peers**, quan sát vài giây. | **Đạt:** cả S (`:16881`) và P (`:16882`) đều tăng Chunks/Data Transferred. Chụp lại trước bước sau. |
| A | Khi L khoảng **20–40%**, **Ctrl+C terminal S**, giữ tracker chạy. | Cắt một nguồn ngay giữa lúc tải. |
| B | Giữ nguyên L; nhìn Progress, Peers và **Execution Logs**. | **Đạt:** kết nối S lỗi nhưng P vẫn gửi mảnh, tiến độ tiếp tục tăng; không khởi động lại L. |
| B | Đợi L **100%**; chạy lệnh hash dưới đây. | Kiểm tra tệp ghép có đúng nội dung ban đầu. |

```bash
# A: hash tệp gốc
sha256sum /tmp/ds02/demo.bin
# B: hash tệp L đã tải
sha256sum /tmp/ds02/l2/demo.bin
```

**Đạt khi hai chuỗi hash giống hệt nhau.** Giữ P chạy để làm nguồn tốt ở mục 4.

## 4. Phát hiện mảnh hỏng và lấy lại từ nguồn tốt

B Ctrl+C terminal L. A Ctrl+C tracker rồi bật lại bằng lệnh ở mục 1.
**Chờ tracker hiện announce của P (`:16882`) rồi mới bắt đầu tải**; P tự đăng ký lại theo heartbeat.

**A bật S cố ý gửi mảnh sai** — tệp gốc vẫn giữ nguyên:

```bash
./build-release/src/peer/PeerApp --tracker http://192.168.1.10:18080 \
  --announce-ip 192.168.1.10 -p 16881 --data-dir /tmp/ds02/s3 \
  --seed /tmp/ds02/demo.bin --corrupt-chunks "$(seq -s, 0 511)" \
  --chunk-delay-ms 20 --webui --webui-port 18081
```

**B bật L với thư mục mới:**

```bash
./build-release/src/peer/PeerApp --tracker http://192.168.1.10:18080 \
  --announce-ip 192.168.1.20 -p 16883 --data-dir /tmp/ds02/l3 \
  --download-limit-mbps 10 --max-downloads 2 --pipeline-depth 1 --io-timeout-ms 6000 \
  --webui --webui-port 18083
```

| Ai | Thao tác | Để làm gì / nhìn gì |
| --- | --- | --- |
| B | Trên L **Add Torrent** JSON, để Output Directory trống → mở **Execution Logs**. | **Đạt:** có `SHA-256 mismatch`, chỉ số mảnh và nguồn sai; mảnh được trả về hàng chờ. |
| B | Mở **Peers** và chờ tải xong. | P tốt vẫn gửi mảnh; L hoàn tất thay vì chấp nhận dữ liệu hỏng. |
| B | Chạy `sha256sum /tmp/ds02/l3/demo.bin`, so với hash gốc ở A. | **Đạt:** hash giống nhau dù đã nhận mảnh lỗi. |

## 5. Nếu cần thêm: pause/resume và tải tiếp sau khi đóng chương trình

Thực hiện **khi L còn đang tải**, P vẫn hoạt động:

| Thao tác của B | Kết quả cần thấy |
| --- | --- |
| Bấm **Pause**, đợi bộ đếm dừng; bấm **Resume**. | Tải tiếp; không mất các mảnh đã xác minh. |
| Ctrl+C terminal L → chạy lại **đúng lệnh của L ở pha hiện tại** → Add Torrent lại cùng JSON, Output Directory vẫn trống. | Tiến độ được nạp lại từ DB và tệp cũ; tải tiếp phần thiếu, không về 0. |

Kết thúc: **Ctrl+C các terminal node còn chạy**.
