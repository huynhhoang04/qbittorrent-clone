# Dựng báo cáo PDF

```bash
QBT_REPORT_FONT_DIR=/duong/dan/toi/font python3 scripts/report/build_report.py
```

Thư mục font cần chứa Times New Roman thật: `times.ttf`, `timesbd.ttf`,
`timesi.ttf`, `timesbi.ttf` (không phân biệt hoa/thường). Khi đã cài font trong
hệ điều hành, có thể bỏ `QBT_REPORT_FONT_DIR`; trình dựng sẽ tìm qua Fontconfig
hoặc các thư mục font thông dụng của Linux/Windows. Không dùng font thay thế.

ReportLab được nạp qua virtualenv riêng trong `scripts/report/.venv/` nếu cần.
Font được nhúng vào PDF; không đưa các tệp font vào Git.

- `content_a.py`, `content_b.py`: nội dung, tên thành viên, bảng và tham chiếu.
- `report_lib.py`: bố cục, Times New Roman và sơ đồ.
- `report_data.py`: đọc hiện vật kiểm thử và CSV theo đường dẫn của repository.
- `docs/bench/results.csv`: số liệu dùng để tính lại các bảng hiệu năng.
- `docs/evidence/`: log và danh sách kết quả được dùng trong báo cáo.

Đầu ra là `docs/report.pdf`. Khi đổi nội dung, trình dựng cập nhật lại mục lục
và số trang. Khi chạy lại kiểm thử hoặc benchmark, cần cập nhật hiện vật và
nội dung nhận xét liên quan trước khi dựng bản nộp.
