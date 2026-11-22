# Chọn bài demo DS02

**Demo tự động trên một máy, dùng tracker và peer thật qua localhost, không mở Web UI. Mỗi bài tự khởi động các node cần thiết, chạy kiểm tra rồi dọn tiến trình. Đầu mỗi bài in rõ THAO TÁC và PASS KHI.**

Chạy từ thư mục gốc repo. Chọn bản build đã có:

```bash
export QBT_BUILD_DIR="$PWD/build-release"
```

| Thầy muốn xem | Lệnh chạy riêng |
| --- | --- |
| Đăng ký và tìm peer | `bash scripts/demo/01-discovery.sh` |
| Tải từ một nguồn, kiểm tra SHA-256 | `bash scripts/demo/02-single-source.sh` |
| Tải đồng thời từ hai nguồn | `bash scripts/demo/03-multi-source.sh` |
| Tắt nguồn giữa chừng, tải tiếp từ nguồn khác | `bash scripts/demo/04-failover.sh` |
| Phát hiện chunk sai và tải lại | `bash scripts/demo/05-corruption.sh` |
| Tắt peer đang tải, mở lại để tiếp tục | `bash scripts/demo/06-crash-resume.sh` |
| Tracker xóa peer mất liên lạc, chờ khoảng 1 phút | `bash scripts/demo/07-tracker-expiry.sh` |

Cũng có thể chọn bằng số: `bash scripts/demo_full.sh 4`. Chạy hết: `bash scripts/demo_full.sh`.

Thêm `--keep-logs` để giữ file tải và log. Nếu FAIL, log được giữ tự động; đường dẫn in ở cuối. Bấm Ctrl+C để dừng và dọn tiến trình. Thông báo `Killed` có thể xuất hiện khi cố tình gây crash.

Demo qua LAN trên hai máy: dùng [hướng dẫn hai máy](../../docs/demo-2-may.md).
