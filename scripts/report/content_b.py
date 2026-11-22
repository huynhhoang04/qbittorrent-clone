"""Report body, part B: sections 11 through 19 (appendix included).

All numbers come from docs/bench/results.csv (scripts/bench.sh) and from the
recorded output of scripts/demo_full.sh and ctest on this machine.
"""

from report_data import ROOT, EVIDENCE, validation

from reportlab.lib.units import mm
from reportlab.platypus import PageBreak, Spacer

from report_lib import (
    code,
    caption,
    h1,
    h2,
    h3,
    numbered,
    p,
    rich,
    rich_bullets,
    spacer,
    table,
)


def section_11_fault():
    return [h1("11. Chịu lỗi và phục hồi"),
        h2("11.1. Mô hình lỗi và phản ứng"),
        rich("Mô hình xử lý gồm crash-stop, khởi động lại tiến trình, kết nối bị ngắt hoặc quá hạn và nội dung mảnh không khớp metadata. Không coi hash là cơ chế chống mọi hành vi Byzantine: tính an toàn còn phụ thuộc metadata tin cậy. Khả năng hoàn tất còn phụ thuộc ít nhất một nguồn khả dụng cho mọi mảnh còn thiếu."),
        table(["Sự cố", "Phát hiện", "Phản ứng / giới hạn"], [
            ("Nguồn bị kill hoặc mất mạng", "EOF/lỗi socket; timeout kết nối hoặc I/O", "Thu hồi in-flight, đưa mảnh về pending và thử nguồn khác; không cần chờ lease tracker"),
            ("Mảnh bị sửa", "SHA-256 không khớp chunk_hashs[i]", "Không ghi mảnh; blacklist cặp (mảnh, peer); tải lại từ nguồn khác"),
            ("Nguồn đều hỏng hoặc không giữ phần còn lại", "Không tiến triển, không còn phiên hữu ích", "Khám phá hữu hạn; báo lỗi, không công bố tệp hoàn tất"),
            ("Leecher bị kill", "Phát hiện khi chạy lại", "Cùng DB, metadata và tệp đích; băm lại mảnh đã lưu; chỉ tải phần thiếu hoặc sai"),
            ("Tracker dừng", "HTTP thất bại", "Các kết nối dữ liệu đã có có thể tiếp tục; khám phá peer mới bị ảnh hưởng"),
            ("Nguồn hết slot", "CHOKE hoặc hết chờ cấp slot", "Thử peer khác; chờ slot hữu hạn, không bảo đảm lịch phục vụ công bằng"),
        ], widths=[.24,.27,.49]),
        caption("Bảng 11.1 — Các lỗi chọn xử lý và điều kiện phục hồi."),
        h2("11.2. Timeout và ngân sách thử lại"),
        rich("Giá trị mặc định trong download_options: connect_timeout_ms = 3000; io_timeout_ms = 4000; max_attempts_per_chunk = 12; max_peer_failures = 3; discovery_attempts = 3; discovery_wait_ms = 500. Một vài giá trị được cấu hình qua CLI. Đây là ngân sách nội bộ, không có nghĩa mọi trường hợp thất bại đều mất đúng 12 × 4 giây."),
        rich("Scheduler kiểm tra nguồn mới theo chu kỳ tối thiểu max(250 ms, discovery_wait_ms), tức mặc định khoảng 500 ms khi vòng lặp có thể chạy. Khám phá trả về peer cũ chưa chứng minh tiến triển. Khi không còn session hữu ích, ngân sách discovery vẫn phải đi tới giới hạn để tránh treo vô hạn. Nếu cạn retry của mảnh, phiên tải kết thúc lỗi rõ ràng."),
        h2("11.3. Checkpoint và phục hồi"),
        numbered(["Nhận PIECE, kiểm tra index, độ dài và SHA-256 theo metadata trước khi ghi.",
                  "Ghi nội dung tại offset i × chunk_size; cập nhật bitfield và trạng thái verified trong DB cục bộ.",
                  "Khi restart, StateManager nạp các mảnh verified; DownloadManager đọc lại nội dung để xác minh.",
                  "Mảnh thiếu hoặc sai bị đánh lại missing; chỉ những mảnh chưa hợp lệ được đưa vào hàng chờ.",
                  "Khi đủ mảnh, băm lại toàn tệp; chỉ báo thành công nếu hash tổng khớp."]),
        rich("SQLite và tệp dữ liệu không cùng một transaction nguyên tử. Vì vậy cần kiểm tra lại nội dung khi resume; WAL không tự làm cho tệp dữ liệu và DB đồng bộ nguyên tử. Thử nghiệm kill tiến trình cũng không tương đương kiểm thử mất điện hoặc hỏng thiết bị lưu trữ."),
        h2("11.4. Minh chứng và giới hạn kiểm chứng"),
        rich("Log đã lưu cho thấy SeederDeathMidDownloadFailsOverToSecondSeeder và CorruptedOnlySourceIsRejectedAndDownloadFails đều PASS. Test RealProcessCrashResumesFromPersistedSqliteState SKIP trên Linux vì nhánh kiểm thử tiến trình này chỉ hỗ trợ Windows. Do đó không thể dùng kết quả ctest Linux để kết luận test crash-resume đã chạy; cần demo kill/restart hoặc log chạy độc lập trên môi trường hỗ trợ (mục 14)."),
    ]



def section_12_security():
    return [h1("12. Bảo mật"),
        h2("12.1. Mô hình tin cậy"),
        rich("Nguyên mẫu giả định các tiến trình do nhóm triển khai trong LAN kiểm soát và metadata được nhận qua kênh tin cậy. Đây là giới hạn triển khai, không miễn trừ các yêu cầu bắt buộc ở §4.5 [1]. SHA-256 kiểm tra nội dung theo metadata; HANDSHAKE khai báo Peer ID và File ID để kiểm tra phiên, chưa chứng minh người gửi sở hữu danh tính đó."),
        table(["Kiểm soát", "Hiện trạng", "Phạm vi bảo vệ"], [
            ("Framing và cấp phát", "Length tối đa 8 MiB; handshake field tối đa 128 byte", "Từ chối frame quá lớn trước cấp phát; không suy rộng giới hạn này cho HTTP upload"),
            ("Dữ liệu tracker", "JSON đúng kiểu; port 1–65535; bitfield boolean", "Loại nhiều đầu vào sai; chưa chứng minh có quota/body limit cho mọi endpoint"),
            ("REQUEST và PIECE", "Kiểm tra chỉ số, kích thước, in-flight và hash", "Hạn chế truy cập ngoài phạm vi và từ chối mảnh không hợp lệ"),
            ("Truy cập SQLite", "Prepared statement và bind; mutex ở lớp ứng dụng", "Tách tham số khỏi câu SQL; không thay thế phân quyền tài nguyên"),
            ("Giới hạn tài nguyên", "Giới hạn phiên/slot; hàng đợi upload hữu hạn; timeout", "Giảm nguy cơ cạn tài nguyên; chưa phải chống DoS đầy đủ"),
            ("Cấu hình", "Không có tài khoản/mật khẩu trong luồng hiện tại; dùng biến môi trường cho cấu hình hỗ trợ", "Khi bổ sung secret cần nạp ngoài Git; .gitignore không chứng minh lịch sử Git sạch secret"),
        ], widths=[.22,.36,.42]),
        caption("Bảng 12.1 — Kiểm soát đã quan sát trong nguồn và giới hạn tương ứng."),
        h2("12.2. Khoảng thiếu so với §4.5"),
        rich("**Xác thực và phân quyền còn thiếu.** Tracker tin Peer ID tự khai; peer khác có thể announce cùng ID và ghi đè endpoint. Dashboard chưa có đăng nhập/token và chưa có ACL cho thao tác seed/download. Không gọi kiểm tra HANDSHAKE là xác thực mật mã hoặc xác thực người dùng."),
        rich('**Đường dẫn và kích thước HTTP.** Mã nguồn có safe_file_name/resolve_within ở các endpoint Web và safe_metadata_name tại PeerNode để chặn tên không an toàn và hạn chế đường dẫn. 15 kiểm tra HTTP/tích hợp bổ sung đã đạt, bao gồm upload hợp lệ, tên tệp thoát thư mục bị từ chối, đường dẫn API nằm ngoài data-dir bị loại, metadata chứa tên không an toàn được đổi về tên cục bộ và tệp tải vẫn đúng hash. Đây chưa phải kiểm chứng mọi trường hợp symlink, quota hoặc HTTP body limit; log chi tiết nằm ở docs/evidence/web-smoke.json.'),
        rich("**Địa chỉ mạng và kênh truyền.** Header X-Forwarded-For/X-Real-IP cần chỉ được tin khi đi qua proxy cho phép; địa chỉ tự khai không phải bằng chứng sở hữu. HTTP tracker và TCP dữ liệu chưa được bảo vệ bằng TLS trong cấu hình hiện tại. TLS được đề bài khuyến khích; xác thực và kiểm soát truy cập là yêu cầu bắt buộc, cần ghi riêng mức đáp ứng."),
        h2("12.3. Ưu tiên hoàn thiện và tiêu chí kiểm tra"),
        rich("Ưu tiên bổ sung token hoặc khóa peer, ACL cho tài nguyên và dashboard; xác minh danh tính announce; chặn path traversal và upload quá lớn; sau đó thêm TLS và hạn mức theo peer/IP. Kiểm thử phải có trường hợp không có/sai token bị từ chối, peer không được phép đọc tệp, filename thoát thư mục bị loại và payload quá giới hạn không gây treo."),
        rich("Mười InvalidDataTest trong mục 14 chứng minh các biện pháp kiểm tra dữ liệu sai ở wire protocol. Chúng không chứng minh hệ thống đã có xác thực, phân quyền hoặc chặn mọi đầu vào HTTP. Nội dung này cần được nói rõ khi bảo vệ."),
    ]



def section_13_implementation():
    return [h1("13. Hiện thực"),
        h2("13.1. Cấu trúc và công nghệ"),
        table(["Thành phần", "Công nghệ / trách nhiệm"], [
            ("src/common", "C++17; OpenSSL EVP cho SHA-256 [5]; metadata; wire protocol; socket wrapper; logger"),
            ("src/tracker", "Crow HTTP server; PeerRegistry trong RAM, bảo vệ bằng mutex"),
            ("src/peer", "Điều phối seed/download; thread/atomic/condition_variable; SQLite cục bộ; CLI/TUI/Web"),
            ("tests", "GoogleTest cho chức năng, concurrency, fault và dữ liệu sai; CTest tổng hợp"),
            ("deploy", "Dockerfile riêng cho peer và tracker; Compose 1 tracker + 3 peer, volume riêng"),
            ("scripts", "Startup, tạo tải, fault injection, demo và benchmark; không là thành phần truyền dữ liệu"),
        ], widths=[.22,.78]),
        caption("Bảng 13.1 — Tổ chức hiện thực; không dùng số dòng làm chỉ số chất lượng."),
        h2("13.2. Vòng đời một peer"),
        numbered(["Nạp .env và biến môi trường cho tracker, cổng, data-dir và tham số tải; CLI có ưu tiên cao nhất.",
                  "Sinh Peer ID; mở data_dir/peer.db và nạp tiến độ đã lưu.",
                  "Khởi động UploadServer; seed thì sinh/nạp metadata và announce; download thì chuẩn bị tệp đích và đăng ký phục vụ phần đã có.",
                  "Scheduler chạy nhiều phiên; callback sau mảnh verified cập nhật tiến độ và announce có throttle.",
                  "Heartbeat định kỳ cập nhật registry; giao diện đọc snapshot trạng thái của PeerNode.",
                  "Dừng có trật tự bằng tín hiệu; kill cưỡng bức chỉ dùng khi kiểm tra lỗi/resume."]),
        rich("Thứ tự ưu tiên cấu hình là CLI, biến môi trường có giá trị không rỗng, tệp .env rồi mặc định. Biến rỗng được xử lý như chưa có giá trị; .env có thể cung cấp giá trị cho trường hợp này. Tham số số sai định dạng phải rơi về mặc định; bộ kiểm thử bổ sung xác nhận các giá trị như 42oops và 1.5 không bị nhận thành số nguyên hợp lệ."),
        h2("13.3. Giao diện và quan sát"),
        rich("CLI là đường chạy dễ tái lập qua tham số và log. TUI (`--tui`) trình bày danh sách tệp, peer và tiến độ trong terminal. Web dashboard (`--webui`) hiển thị piece map và điều khiển seed/download/pause/resume. `GET /api/status` trả Peer ID, danh sách torrent, completed_chunks, download_peers, upload_peers và bộ đếm dữ liệu; đây là công cụ chứng minh peer vẫn đang tải nhưng đã upload."),
        rich("Mỗi tiến trình cần ghi log riêng với ánh xạ tên vai trò → máy/IP/port/Peer ID. Dòng khởi động liên kết log với node; timestamp hỗ trợ dựng lại diễn biến. Không giả định đồng hồ hai máy tạo được một thứ tự nhân quả chính xác; đối chiếu thêm File ID, chunk index và endpoint. Kịch bản demo ưu tiên bằng chứng trạng thái và hash thay cho chỉ nhìn thanh tiến độ."),
        h2("13.4. Build và chạy lại"),
        code("""cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j2
./build-release/src/tracker/TrackerApp -p 8080
# Terminal khác: IP_A là IP LAN của máy chạy tracker
./build-release/src/peer/PeerApp --tracker http://IP_A:8080 \\
  --announce-ip IP_A -p 6881 --data-dir ./demo/seed \\
  --seed ./demo/demo.bin --chunk-size 262144"""),
        h2("13.5. Trạng thái lưu trữ"),
        rich("Mỗi peer có DB và tệp riêng, không mount chung DB giữa các node. Ghi dữ liệu theo offset giúp nhận mảnh không theo thứ tự nhưng giữ đúng vị trí trong tệp. Các thao tác FileManager được khóa cục bộ; do đó nhiều kết nối mạng không có nghĩa các lần ghi đĩa đều chạy song song. WAL hỗ trợ giao dịch SQLite trên một máy [4], không biến SQLite thành database phân tán."),
    ]



def section_14_testing():
    return [h1("14. Kiểm thử và kết quả"),
        h2("14.1. Nguồn bằng chứng"),
        rich('Kết quả được lưu trong `docs/evidence/ctest-release.log` và `docs/evidence/ctest-debug.log`, trên mã nguồn được định danh trong `docs/evidence/validation.json`. Cả hai cấu hình đăng ký 48 test: 47 PASS, 1 SKIP dành riêng cho Windows. Bản Debug bật ASan/UBSan. Phụ lục 19.2 lưu danh sách từng test; các kết quả phản ánh cấu hình và phiên bản được kiểm thử.'),
        table(["Nhóm / yêu cầu §9", "Số bài đăng ký", "Kết quả đã lưu", "Phạm vi"], [
            ("Chức năng/cấu hình: tối thiểu 8", "26", "26 PASS", "20 bài lõi + 6 EnvTest: nạp .env, ưu tiên môi trường, giá trị trống, số sai định dạng và clamping"),
            ("Đồng thời: tối thiểu 2", "9", "9 PASS", "Nhiều leecher/nguồn; peer chưa đủ tệp phục vụ peer khác; slot và stress"),
            ("Lỗi–phục hồi: tối thiểu 2", "3", "2 PASS, 1 SKIP", "Mất nguồn và nguồn hỏng; crash-resume chỉ hỗ trợ Windows trong test"),
            ("Bảo mật/dữ liệu sai: tối thiểu 2", "10", "10 PASS", "Oversize/truncated frame, malformed handshake, index sai và slot leak"),
            ("Tổng CTest", "48", "47 PASS, 1 SKIP", "SKIP được tách khỏi số bài thực sự đã chạy và đạt"),
            ("Hiệu năng: tối thiểu 1 thí nghiệm", "12 cấu hình", "CSV: verified=yes ở 12 dòng", "3 số nguồn + 3 kích thước + 6 cấu hình đồng thời"),
        ], widths=[.25,.13,.21,.41]),
        caption("Bảng 14.1 — Mức bao phủ tối thiểu và kết quả từ hiện vật đã lưu."),
        h2("14.2. Hai tình huống đồng thời tiêu biểu"),
        rich("**Hai nguồn, một leecher.** TwoSeedersOneLeecher kiểm tra nhiều nguồn có đóng góp và đầu ra đúng hash; số phiên không vượt giới hạn. Không kết luận đa nguồn chỉ vì /peers trả hai địa chỉ: phải có ít nhất hai Peer ID với chunks_downloaded > 0 và cùng nằm trong phiên tải."),
        rich("**Vừa tải vừa phục vụ.** LeecherServesAnotherLeecher và PartialPeersServeEachOtherWhileDownloading kiểm tra trao đổi dữ liệu giữa peer chưa hoàn tất. Trong demo, cần chụp cùng thời điểm completed_chunks < num_chunks và uploaded_bytes > 0 hoặc upload_peers có chunks_uploaded > 0; chỉ thấy peer upload sau 100% không đủ chứng minh yêu cầu này."),
        h2("14.3. Các lỗi và dữ liệu sai"),
        rich("SeederDeathMidDownloadFailsOverToSecondSeeder: ngắt nguồn giữa phiên và hoàn tất từ nguồn còn sống, đầu ra đúng hash. CorruptedOnlySourceIsRejectedAndDownloadFails: khi chỉ còn nguồn hỏng thì phải báo lỗi và không coi tệp sai là thành công. CorruptedChunksAreDetectedRequeuedAndRefetched thuộc nhóm concurrency nhưng bổ sung bằng chứng lỗi dữ liệu; không cộng nó lần nữa vào tổng số test."),
        rich("InvalidDataTest kiểm tra giới hạn cấp phát trước length quá lớn, frame cụt không treo, bắt tay sai bị từ chối và REQUEST ngoài phạm vi không làm rò slot. RealProcessCrashResumesFromPersistedSqliteState SKIP trên Linux với thông báo 'process kill/resume is implemented for Windows only'; cần kịch bản restart độc lập để xác nhận hành vi đó trên LAN."),
        h2("14.4. Đối chiếu bảy tình huống DS02 §6.4"),
        table(["Tình huống bắt buộc", "Điều kiện chấp nhận và bằng chứng"], [
            ("Tạo metadata và chia sẻ", "Metadata đúng trường; cùng File ID; seeder có đủ mảnh trong /peers"),
            ("Tải song song từ ít nhất 2 peer", "≥2 nguồn góp mảnh; ít nhất hai phiên có hoạt động trong thời gian giao nhau; hash cuối đúng"),
            ("Upload khi chưa tải xong", "Snapshot cùng thời điểm: 0 < completed_chunks < num_chunks và uploaded_bytes > 0"),
            ("Dừng nguồn giữa lúc tải", "Lưu PID và thời điểm kill; tiến độ còn dưới 100%; session nguồn lỗi"),
            ("Chuyển sang nguồn khác", "Sau kill vẫn tăng verified; log/state xác định nguồn còn lại góp mảnh"),
            ("Phát hiện mảnh sai", "Có SHA-256 mismatch; mảnh không được chấp nhận từ nguồn sai; retry từ nguồn tốt"),
            ("Ghép và xác minh hash tổng", "Ghi đúng offset; sha256sum đầu ra khớp file_hash, không chỉ đúng kích thước"),
        ], widths=[.35,.65]),
        caption("Bảng 14.2 — Tiêu chí demo trực tiếp; không đồng nhất với số thứ tự demo_full.sh."),
        rich('`scripts/demo_full.sh` đã chạy đạt 7/7 trên bản Release; log đầy đủ được lưu tại docs/evidence/demo-full.log. Các trường hợp gồm discovery, tải một/đa nguồn, mất seeder, mảnh hỏng, crash-resume và lease. Kết quả crash-resume của script bổ sung minh chứng Linux cho bài CTest bị SKIP. Các kịch bản không khớp một-một với bảy dòng §6.4: yêu cầu upload khi chưa hoàn tất được kiểm tra riêng bởi các test đồng thời. Việc diễn tập thực tế trên hai máy LAN vẫn là bước triển khai tách biệt với các phép đo loopback này.'),
    ]



def section_15_performance():
    import csv
    from pathlib import Path
    rows = list(csv.DictReader((ROOT / "docs/bench/results.csv").open()))
    sources=[r for r in rows if r['dimension']=='sources']
    sizes=[r for r in rows if r['dimension']=='filesize']
    concurrency=[r for r in rows if r['dimension']=='concurrency']
    base=int(sources[0]['elapsed_ms'])
    fmt=lambda x: ("%.3f" % x).replace('.',',')
    return [h1("15. Đánh giá hiệu năng"),
        h2("15.1. Thiết kế thí nghiệm và đơn vị"),
        rich("Nguồn số liệu là `docs/bench/results.csv` có 12 dòng, được lưu từ scripts/bench.sh. Script chạy các tiến trình độc lập trên loopback, bản Release, mặc định chunk 262144 byte (256 KiB), seeder delay 50 ms/mảnh; phép đo bắt đầu trước khi chạy leecher và kết thúc khi tiến trình hoàn tất. Download giới hạn 8 nguồn, pipeline 4 trong cấu hình benchmark; mỗi nguồn ở thí nghiệm số nguồn có 1 upload slot."),
        rich("Môi trường được ghi nhận kèm bộ số liệu: Intel Core i5-11300H, 8 luồng, khoảng 15 GiB RAM, EndeavourOS, GCC 16.1.1. CSV không tự ghi CPU/OS/compiler hoặc thời điểm chạy, vì vậy đây là mô tả môi trường kèm phép đo, chưa phải manifest tự xác thực. Release cache hiện có xác nhận -O3 -DNDEBUG. Bộ số liệu chưa kèm log lặp lại cho từng cấu hình."),
        rich("Chuẩn hóa đơn vị: 1 MiB = 1048576 byte; 1 KiB = 1024 byte. CSV 104857600 byte là **100 MiB**. Thông lượng Q = bytes / 1048576 / (elapsed_ms / 1000), đơn vị MiB/s; speedup S_p = T_1/T_p; hiệu suất E_p = S_p/p. Bộ nhớ VmHWM tính bằng KiB trong hiện vật, chia 1024 để ra MiB."),
        h2("15.2. Tăng số nguồn"),
        table(["Nguồn", "Tệp", "Thời gian (s)", "MiB/s", "Speedup", "RSS (MiB)"],
              [(r['configuration'].split('-')[0],str(int(r['bytes'])//1048576)+' MiB',fmt(int(r['elapsed_ms'])/1000),fmt(int(r['bytes'])/1048576/(int(r['elapsed_ms'])/1000)),fmt(base/int(r['elapsed_ms']))+'×',fmt(int(r['peak_rss_kb'])/1024)) for r in sources], widths=[.10,.17,.20,.17,.18,.18]),
        caption("Bảng 15.1 — Tính lại trực tiếp từ CSV; contributor tương ứng 1, 2 và 3 nguồn."),
        rich('Trong điều kiện đo, tăng số nguồn giúp giảm thời gian hoàn tất; speedup được tính từ thời gian một nguồn chia thời gian tương ứng trong Bảng 15.1. Kết quả phù hợp với việc phục vụ độc lập khi mỗi nguồn bị thêm cùng độ trễ. Không suy ra hệ thống luôn tăng tốc tuyến tính: ghi tệp vẫn có khóa và WAN có thể bị giới hạn bởi uplink, RTT, disk hoặc CPU.'),
        h2("15.3. Thay đổi kích thước tệp"),
        table(["Tệp (MiB)", "Số mảnh", "Thời gian (s)", "MiB/s", "Delay thuần (s)"],
              [(str(int(r['bytes'])//1048576),str((int(r['bytes'])+262143)//262144),fmt(int(r['elapsed_ms'])/1000),fmt(int(r['bytes'])/1048576/(int(r['elapsed_ms'])/1000)),fmt(((int(r['bytes'])+262143)//262144)*.05)) for r in sizes], widths=[.18,.15,.23,.20,.24]),
        caption("Bảng 15.2 — Một nguồn; cột delay thuần là số mảnh × 50 ms, không phải dự báo WAN."),
        rich('Hiệu số giữa thời gian đo và tổng delay thuần gồm chi phí khởi động, scheduler, socket, đĩa và xác minh. Dữ liệu hiện có không tách riêng từng thành phần hoặc chứng minh chi phí luôn cố định. Hash toàn tệp vẫn có chi phí đọc O(kích thước tệp).'),
        h2("15.4. Nhiều leecher đồng thời"),
        table(["Cấu hình", "Tổng MiB", "Thời gian cả nhóm (s)", "MiB/s tổng"],
              [(r['configuration'],str(int(r['bytes'])//1048576),fmt(int(r['elapsed_ms'])/1000),fmt(int(r['bytes'])/1048576/(int(r['elapsed_ms'])/1000))) for r in concurrency], widths=[.31,.16,.29,.24]),
        caption("Bảng 15.3 — Tệp 20 MiB/leecher; tổng bytes = kích thước tệp × số leecher."),
        rich("Thời gian lấy mốc khi cả nhóm hoàn tất; thông lượng là dữ liệu ứng dụng cộng trên toàn nhóm chia thời gian, không phải tốc độ uplink của seeder hay băng thông từng máy. CSV ghi contributors='-' và RSS=0 ở nhóm này, nghĩa là không có số đo tương ứng để kết luận. Việc leecher hỗ trợ nhau được kiểm chứng riêng bằng test và snapshot demo, không suy ra chỉ từ bảng thông lượng."),
        h2("15.5. Hạn chế đo lường và cách bổ sung"),
        rich("Mỗi cấu hình chỉ có một mẫu; không có median, p95, độ lệch chuẩn hay khoảng tin cậy. Độ trễ nhân tạo là thời gian chờ ở ứng dụng, không mô phỏng đầy đủ RTT, mất gói và băng thông WAN. Các tiến trình chia sẻ disk/cache của một máy. Thứ tự chạy và nguồn dữ liệu ngẫu nhiên cũng có thể tác động kết quả."),
        rich("Để đánh giá chặt hơn: chạy ≥5 lần/cấu hình với warm-up riêng, lưu commit và môi trường; đo median/p95, CPU, RSS và NIC bytes; triển khai hai máy có IP thật; đo 1–3 nguồn với cùng file và chunk_size; kiểm tra hash sau mỗi lần. Nếu dùng netem phải ghi cấu hình mạng. Bộ dữ liệu hiện có đáp ứng một thí nghiệm hiệu năng ở quy mô nguyên mẫu, chưa cho phép kết luận về quy mô lớn."),
    ]



def section_16_limitations():
    return [h1("16. Hạn chế và hướng phát triển"),
        h2("16.1. Các giới hạn ảnh hưởng trực tiếp"),
        table(["Hạn chế", "Hệ quả", "Hướng khắc phục"], [
            ("Thiếu xác thực và ACL", "Chưa đáp ứng trọn vẹn §4.5; Peer ID và dashboard có thể bị dùng trái phép", "Token/khóa peer, ACL theo File ID; kiểm thử từ chối truy cập"),
            ("Hardening đường dẫn mới, giới hạn HTTP chưa đầy đủ", "Guard mới chưa có log kiểm chứng; payload lớn vẫn cần quota", "Test traversal/symlink trên bản cuối; quota và body limit"),
            ("Tracker đơn, registry trong RAM", "Discovery bị ngừng khi tracker lỗi; restart cần re-announce", "Nhiều tracker; sao chép trạng thái hoặc DHT"),
            ("Peer ID chưa bền; announce chưa có version", "Restart sinh ID mới; announce cũ có thể cập nhật snapshot lùi", "Persist identity; epoch/sequence và xác minh danh tính"),
            ("File ID chỉ hash nội dung", "Cùng tệp nhưng metadata chia mảnh khác có thể bị xếp cùng swarm", "Định danh gồm bố cục metadata; kiểm tra cùng layout"),
            ("Không NAT traversal/TLS", "Cần endpoint TCP truy cập được; nội dung có thể bị nghe lén", "Cấu hình NAT/VPN phù hợp; TLS; không gán đây là tính năng hiện có"),
            ("Chưa có công bằng theo đóng góp", "CHOKE/UNCHOKE giới hạn slot, chưa là tit-for-tat", "Chính sách theo tốc độ/đóng góp; rate limit"),
            ("Chưa có manifest đo và nhiều mẫu", "Không đánh giá được độ biến thiên hoặc độ ổn định WAN", "Lưu môi trường/commit/log; lặp nhiều lần trên nhiều máy"),
        ], widths=[.29,.35,.36]),
        caption("Bảng 16.1 — Hạn chế gắn với hậu quả và việc cần làm cụ thể."),
        h2("16.2. Thứ tự ưu tiên"),
        rich("Ưu tiên đầu tiên là các yêu cầu bắt buộc còn thiếu về bảo mật và bộ minh chứng trên commit nộp cuối. Tiếp theo là danh tính bền, validation metadata và File ID bao gồm bố cục; sau đó mới tăng khả dụng tracker, công bằng/băng thông và đo trên mạng thực. DHT, magnet link, replicated tracker và mã hóa là các hướng nâng cao, không được ghi như đã hiện thực."),
        rich("Rarest-first và resume đã có trong thiết kế hiện tại, nhưng cần chứng minh bằng test phù hợp. Không đưa chúng vào danh sách 'chưa có' cùng DHT. Việc giữ lại giới hạn có căn cứ giúp tách mức nguyên mẫu học thuật khỏi một sản phẩm BitTorrent triển khai công cộng."),
    ]



def section_17_conclusion():
    return [h1("17. Kết luận"),
        rich("Đồ án DS02 đã hiện thực nguyên mẫu P2P với tracker quản lý danh sách peer, metadata SHA-256, tải mảnh đa nguồn và phục vụ mảnh đã xác minh. Mọi node giao tiếp qua mạng; state và tệp của từng peer được lưu riêng. Mô hình demo phải có ít nhất 3 peer ngoài tracker, kể cả khi chỉ dùng hai máy vật lý."),
        rich('Bằng chứng đã lưu gồm 47 bài kiểm thử đạt ở mỗi cấu hình Release và Debug, một bài CTest crash-resume bị bỏ qua trên Linux, 7/7 demo tự động, 15 kiểm tra HTTP bổ sung và 12 cấu hình hiệu năng có hash đúng. Script demo xác nhận crash-resume trên tiến trình Linux thật. Các phép đo vẫn sử dụng loopback, chưa xác nhận hiệu năng WAN hoặc quy mô lớn; bảy tiêu chí DS02 được đối chiếu riêng ở Bảng 14.2.'),
        rich("Bài học thiết kế là tách trạng thái khám phá có thể trễ khỏi bất biến toàn vẹn nội dung, phân biệt timeout nghi ngờ lỗi với bằng chứng peer chết, và kiểm tra lại dữ liệu khi phục hồi checkpoint. Hash không thay cho xác thực, bitfield một chủ sở hữu không mặc nhiên là CRDT, và WAL không thay cho đồng bộ phân tán."),
        rich("Các cơ chế lõi phục vụ mục tiêu học tập về discovery, concurrency, consistency và recovery. Mức đáp ứng bảo mật vẫn một phần; cần hoàn thiện xác thực, ACL và kiểm soát đường dẫn trước khi tuyên bố đáp ứng toàn bộ yêu cầu chung. Kết quả cần được chạy lại và gắn với commit/tag nộp cuối để tránh nhầm hiện vật cũ với bản đang thay đổi."),
    ]



def section_18_references():
    return [h1("18. Tài liệu tham khảo"),
        rich("[1] Học phần Hệ thống phân tán, *COURSE PROJECT SPECIFICATION*, bản PDF cung cấp trong kho; trọng tâm §4, §6 (DS02), §9–13. Nguồn xác định yêu cầu, không suy diễn thêm yêu cầu WAN của DS01 sang DS02."),
        rich("[2] Kho mã nguồn nhóm: https://github.com/huynhhoang04/qbittorrent-clone — tham chiếu mã nguồn hoàn thiện tại 72a5d0c, README.md, src/, tests/, scripts/, deploy/; hiện vật kết quả: docs/bench/results.csv và docs/evidence/ và log CTest ngày 22/11/2026. Các thay đổi working tree sau mốc này cần kiểm tra lại."),
        rich("[3] Bram Cohen, *The BitTorrent Protocol Specification*, BEP 3. https://www.bittorrent.org/beps/bep_0003.html — tham khảo mô hình tracker, metainfo và trao đổi mảnh; giao thức của đồ án có các khác biệt ở §8.1. Truy cập 22/11/2026."),
        rich("[4] SQLite, *Write-Ahead Logging*. https://sqlite.org/wal.html — tham khảo mô hình readers/writer và phạm vi WAL cục bộ. Truy cập 22/11/2026."),
        rich("[5] OpenSSL, *EVP_DigestInit / EVP_DigestUpdate / EVP_DigestFinal*. https://docs.openssl.org/3.0/man3/EVP_DigestInit/ — API digest được dùng qua lớp SHA256 của đồ án. Truy cập 22/11/2026."),
        rich("[6] Thư viện được tích hợp trong mã: Crow (HTTP), nlohmann/json (JSON), GoogleTest (test), SQLite, OpenSSL và ncurses (TUI). Đối chiếu include/CMake và giấy phép đúng phiên bản khi đóng gói bản nộp. Không coi việc sử dụng thư viện là bằng chứng nhóm đã tự viết lại các thư viện đó."),
        rich("Các hình kiến trúc và trình tự trong báo cáo là sơ đồ mô tả hiện thực của nhóm. Lõi điều phối peer, registry, scheduler, retry và lưu trạng thái thể hiện trong mã nguồn của kho; không sử dụng qBittorrent hoàn chỉnh rồi chỉ thay giao diện. Theo §13 [1], nhóm phải tự kiểm chứng, hiểu và chịu trách nhiệm về mọi nội dung có hỗ trợ từ công cụ AI."),
    ]



def section_19_appendix():
    import json
    from pathlib import Path
    folder=EVIDENCE
    results=json.loads((folder/'test-summary.json').read_text())
    return [PageBreak(),h1("19. Phụ lục"),
        h2("19.1. Lệnh tái kiểm chứng trước khi nộp"),
        code("""# Chạy sau khi chốt code; giữ log gắn với commit cuối.
mkdir -p docs/evidence
ctest --test-dir build --output-on-failure > docs/evidence/ctest.txt 2>&1
# Kiểm tra exit code, số PASS/FAIL/SKIP trong log.
QBT_BUILD_DIR="$PWD/build-release" ./scripts/demo_full.sh \\
  > docs/evidence/demo-full.txt 2>&1
QBT_BUILD_DIR="$PWD/build-release" BENCH_CSV=docs/bench/results.csv \\
  ./scripts/bench.sh > docs/evidence/bench.txt 2>&1
# Giữ thêm git rev-parse HEAD, môi trường và lệnh đã dùng."""),
        rich("Các lệnh trên mô tả cách tái lập quá trình đánh giá. Khi thu log qua pipeline trong Bash cần bật pipefail để giữ trạng thái thất bại của lệnh kiểm thử."),
        h2("19.2. Tóm tắt phiên CTest đã lưu"),
        rich('Nguồn: docs/evidence/ctest-release.log; đối chiếu thêm docs/evidence/ctest-debug.log. Cả hai lần chạy có 48 mục: 47 PASS và 1 SKIP. Danh sách bên dưới được trích từ log Release; metadata và hash các hiện vật được lưu tại docs/evidence/validation.json.'),
        table(["#", "Tên test (suite.case)", "Kết quả"],[(str(i+1),n,status) for i,(n,status) in enumerate(results)], widths=[.06,.81,.13]),
        caption("Bảng 19.1 — 48 mục trong log; 47 PASS và 1 SKIP, không có FAIL trong phiên đánh giá cuối."),
        h2("19.3. Dữ liệu thô hiệu năng"),
        code((ROOT/'docs/bench/results.csv').read_text().strip()),
        rich("CSV được chụp lại từ hiện vật có sẵn để tính các bảng mục 15. 'verified=yes' là trường kết quả của script; contributors '-' là chưa đo, RSS 0 ở dòng đồng thời là thiếu mẫu, không phải tiến trình không dùng RAM."),
        h2("19.4. Tham số vận hành quan trọng"),
        table(["CLI", "Mặc định / ý nghĩa"], [
            ("--tracker", "TRACKER_URL hoặc http://localhost:8080; demo nhiều máy phải thay bằng IP tracker"),
            ("-p / --port", "Peer 6881; tracker 8080; 0 để OS cấp cổng nếu đường chạy hỗ trợ"),
            ("--data-dir / --output-dir", "DB/trạng thái riêng và nơi đặt tệp; giữ nguyên cả hai để resume"),
            ("--announce-ip", "IP/hostname mà các peer khác kết nối được; có thể dùng PEER_HOST"),
            ("--chunk-size", "262144 byte khi tạo metadata; dùng chung metadata cho mọi peer trong demo"),
            ("--max-downloads / --max-uploads", "Mặc định 5/5; giới hạn nguồn/slot, không phải rate limit"),
            ("--pipeline-depth / --io-timeout-ms", "Mặc định 3 / 4000; timeout quá ngắn dễ loại nhầm nguồn bận"),
            ("--chunk-delay-ms / --corrupt-chunks", "Mặc định 0 / rỗng; chỉ thêm delay/hỏng mảnh trong test/demo"),
            ("--webui / --webui-port", "Dashboard tắt mặc định; cổng mặc định 8081"),
            ("--exit-after-download", "Thoát sau tải: không dùng cho peer cần tiếp tục làm nguồn demo"),
        ], widths=[.34,.66]),
        caption("Bảng 19.2 — Tham số lấy từ CLI; chạy --help trên binary chốt để xác nhận."),

        h2("19.5. Hiện vật bàn giao"),
        table(["Hạng mục", "Hiện trạng / việc cần xác nhận"], [
            ("README, src, tests, scripts, deploy, .env.example", "Có trong kho; cần xác nhận hướng dẫn chạy trên môi trường sạch"),
            ("docs/report.pdf", "Báo cáo này: thông tin 3 thành viên, 19 mục và kết quả có nguồn"),
            ("docs/slides.pdf", "Chưa thấy trong snapshot; đề bài cho phép slides nếu có"),
            ("Commit của từng thành viên", "Có lịch sử đối chiếu ở mục 2; mỗi người mở đúng phần việc khi vấn đáp"),
            ("Final tag/release và quyền truy cập", "Nhóm kiểm tra trên commit cuối và cấp quyền giảng viên nếu repo private; không suy ra từ số phiên bản CMake"),
            ("Demo trực tiếp", "≥3 peer + tracker; mạng; đồng thời; lỗi live; log; cả ba thành viên tham gia"),
        ], widths=[.34,.66]),
        caption("Bảng 19.3 — Các hiện vật nộp bài và xác nhận vận hành cần có."),
    ]
