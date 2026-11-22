# Kết quả kiểm tra bản nộp

Đánh giá trên Linux ngày 22/11/2026. Mã ứng dụng được kiểm tra tại commit
`72a5d0c`; [validation.json](validation.json) lưu môi trường, hash cây nguồn
`src/` + `tests/` và hash các hiện vật.

| Kiểm tra | Kết quả | Minh chứng |
| --- | --- | --- |
| CTest Release | 48 đăng ký, 47 PASS, 1 SKIP | [ctest-release.log](ctest-release.log) |
| CTest Debug + ASan/UBSan | 48 đăng ký, 47 PASS, 1 SKIP | [ctest-debug.log](ctest-debug.log) |
| Demo tự động | 7/7 PASS | [demo-full.log](demo-full.log) |
| Cấu hình và HTTP/Web | 15/15 PASS | [web-smoke.json](web-smoke.json) |
| Hiệu năng | 12/12 cấu hình, hash đúng | [bench.log](bench.log), [CSV](../bench/results.csv) |
| Docker Compose | Cấu hình hợp lệ | `docker compose -f deploy/docker-compose.yml config --quiet` |

CTest bỏ qua `RealProcessCrashResumesFromPersistedSqliteState` trên Linux vì
test này chỉ hỗ trợ Windows. Demo 6 đã xác nhận kill/restart và resume của tiến
trình Linux thật: tệp hoàn tất đúng SHA-256 và không tải lại toàn bộ.

Các dòng `Killed` trong log demo là thao tác fault injection có chủ đích. Kết quả
cuối được xác định bằng bảng PASS/FAIL và mã thoát của script.

`env-regression-before-fix.log` ghi lại lỗi số nguyên như `42oops`/`1.5` trước
khi sửa, nhằm chứng minh kiểm thử phát hiện lỗi. Kết quả sau sửa nằm trong hai
log CTest cuối, đều PASS. Kiểm tra lại HTTP bằng:

```bash
QBT_BUILD_DIR="$PWD/build-release" python3 scripts/web_smoke.py
```

Chưa chạy container do Docker daemon không hoạt động và việc khởi động dịch vụ
cần mật khẩu sudo. Bộ này cũng chưa phải kết quả diễn tập hai máy LAN thật.
Windows được kiểm tra qua job GitHub Actions riêng, không tính vào số liệu
CTest Linux trên đây. Giới hạn xác thực, phân quyền và TLS được nêu trong báo cáo.
