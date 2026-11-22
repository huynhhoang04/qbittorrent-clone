"""Report body, part B: sections 11 through 19 (appendix included).

All numbers come from docs/bench/results.csv (scripts/bench.sh) and from the
recorded output of scripts/demo_full.sh and ctest on this machine.
"""

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
    return [
        h1("11. Chịu lỗi và phục hồi"),
        h2("11.1. Mô hình lỗi và ngân sách lỗi"),
        rich(
            "Hệ thống xử lý bốn tình huống lỗi mà đề bài yêu cầu, cộng thêm hai tình huống vận hành thực tế. Mỗi "
            "tình huống đều có một ngưỡng phát hiện và một ngân sách thử lại hữu hạn, để một sự cố cục bộ không "
            "biến thành vòng lặp vô hạn."
        ),
        table(
            ["Tình huống", "Cách phát hiện", "Phản ứng", "Ngưỡng"],
            [
                ("Peer rời mạng giữa chừng", "`recv` trả 0/lỗi, hoặc hết `io_timeout_ms`",
                 "Đưa mảnh đang bay trở lại hàng đợi; chọn peer khác; blacklist peer đó cho mảnh đó",
                 "timeout 4000 ms, tối đa 12 lần/mảnh, 3 lỗi/peer"),
                ("Dữ liệu hỏng", "SHA-256 mảnh không khớp metadata",
                 "Không ghi mảnh; blacklist peer cho mảnh đó; yêu cầu lại từ peer khác",
                 "ngay lập tức, cùng ngân sách retry"),
                ("Tiến trình bị kill", "Không cần phát hiện — khởi động lại và nạp lại",
                 "Đọc SQLite, xác minh lại từng mảnh đã ghi, tải tiếp phần còn thiếu",
                 "resume, không tải lại từ đầu"),
                ("Tracker sập", "HTTP lỗi / timeout",
                 "Các luồng đang chạy tiếp tục; peer vẫn thử hỏi lại theo chu kỳ",
                 "heartbeat 15 s"),
                ("Seeder hết slot", "Nhận `CHOKE`",
                 "Chuyển sang peer khác trong danh sách đã biết",
                 "`choke_wait_ms` = 2000 ms"),
                ("Mảnh không còn nguồn nào giữ", "Không còn peer khả dụng cho mảnh đó",
                 "Peer vẫn sống, hỏi lại tracker theo chu kỳ; không kết thúc sớm giả tạo",
                 "khám phá lại mỗi 250 ms"),
            ],
            widths=[0.22, 0.24, 0.36, 0.18],
        ),
        caption("Bảng 11.1 — Sáu tình huống lỗi, ngưỡng phát hiện và phản ứng."),
        spacer(2),
        h2("11.2. Ba cơ chế chính"),
        rich(
            "**Timeout và retry có giới hạn.** Mỗi thao tác socket có hạn thời gian; mỗi mảnh có ngân sách 12 lần thử; "
            "mỗi peer có ngân sách 3 lỗi trước khi bị loại khỏi phiên. Khi một mảnh cạn ngân sách, nó được đưa vào "
            "`failed_chunks` và cả phiên tải kết thúc với lỗi rõ ràng thay vì treo vô hạn."
        ),
        rich(
            "**Blacklist theo từng mảnh, không theo peer.** Khi phát hiện mảnh hỏng, hệ thống không loại toàn bộ peer "
            "mà chỉ ghi “peer P không được phục vụ mảnh i”. Đây là quyết định quan trọng: một peer có thể hỏng đúng "
            "một mảnh trên đĩa (lỗi đọc sector) nhưng vẫn phục vụ tốt 399 mảnh còn lại. Blacklist toàn cục sẽ làm "
            "mất một nguồn lành mạnh, còn blacklist theo mảnh giữ được phần lớn giá trị của nó."
        ),
        rich(
            "**Checkpoint và resume.** Trạng thái mỗi mảnh được ghi vào SQLite **ngay sau** khi xác minh thành công, "
            "không chờ hết tệp. Khi tiến trình khởi động lại, `StateManager::load_from_db` nạp bitfield từ bảng "
            "`chunks`, rồi `verify_existing_chunks` đọc lại từng mảnh đã đánh dấu để chắc chắn nội dung trên đĩa vẫn "
            "khớp metadata trước khi tin tưởng nó. Chi phí này chỉ trả một lần lúc khởi động và bảo vệ khỏi trường "
            "hợp tệp bị sửa hoặc đĩa lỗi."
        ),
        code(
            """// Khởi động lại: nạp từ SQLite, rồi xác minh lại nội dung trên đĩa
auto torrents = db->get_all_torrents();
for (const auto& t : torrents) {
    vector<bool> bf(t.num_chunks, false);
    for (const auto& c : db->get_chunks(t.file_id))
        if (c.status == "verified") bf[c.chunk_index] = true;
    bitfields[t.file_id] = bf;          // trạng thái khôi phục
}
// download_manager.cpp — chỉ tải những mảnh còn thiếu
pending_chunks = state_manager->get_missing_chunks(file_id);"""
        ),
        h2("11.3. Kết quả kiểm chứng"),
        rich(
            "Bốn bài kiểm thử trong `tests/test_fault_recovery.cpp` bao phủ các tình huống trên. Bài "
            "`SeederDeathMidDownloadFailsOverToSecondSeeder` giết seeder giữa chừng và yêu cầu tệp vẫn hoàn tất với "
            "mã băm đúng; bài `CorruptedOnlySourceIsRejectedAndDownloadFails` kiểm tra trường hợp **chỉ** có nguồn "
            "hỏng — hệ thống phải thất bại rõ ràng chứ không được ghi một tệp sai. Bài "
            "`RealProcessCrashResumesFromPersistedSqliteState` khởi động một `PeerApp` thật, kill bằng `SIGKILL` "
            "giữa lúc tải, rồi khởi động lại với cùng thư mục trạng thái; bài này chỉ được biên dịch trên Windows "
            "nên bị bỏ qua trên Linux (xem mục 14.3). Kịch bản demo 4 và 6 kiểm chứng cùng hai tình huống này trên "
            "bản Release, độc lập với bộ unit test."
        ),
    ]


def section_12_security():
    return [
        h1("12. Bảo mật"),
        h2("12.1. Kiểm soát đã hiện thực"),
        table(
            ["Nhóm kiểm soát", "Cơ chế", "Vị trí"],
            [
                ("Xác thực phiên", "`HANDSHAKE` bắt buộc là khung đầu tiên; `peer_id` + `file_id` phải khớp trước khi phục vụ",
                 "`upload_server.cpp`"),
                ("Toàn vẹn dữ liệu", "SHA-256 từng mảnh + toàn tệp; sai lệch bị loại, không bao giờ ghi",
                 "`download_manager.cpp`"),
                ("Giới hạn kích thước", "Khung tối đa 8 MB; `peer_id`/`file_id` tối đa 128 byte; kiểm tra trước khi cấp phát",
                 "`protocol.cpp`"),
                ("Kiểm tra dữ liệu vào", "Âm thanh JSON phải đúng kiểu; cổng 1–65535; `bitfield` là mảng boolean",
                 "`tracker_server.cpp`"),
                ("Chỉ số trong phạm vi", "`REQUEST` ngoài `num_chunks` bị đóng kết nối và giải phóng slot",
                 "`upload_server.cpp`"),
                ("Chống SQL injection", "Toàn bộ truy vấn dùng `sqlite3_prepare_v2` + bind tham số",
                 "`database.cpp`"),
                ("Giới hạn tài nguyên", "`max_active_downloads/uploads`, hàng đợi 64 kết nối, slot có timeout",
                 "`upload_server.cpp`"),
                ("Không lưu bí mật", "`.env` bị `.gitignore`; chỉ `.env.example` được commit; kho không chứa secret",
                 "`.gitignore`, `.env.example`"),
            ],
            widths=[0.20, 0.58, 0.22],
        ),
        caption("Bảng 12.1 — Các kiểm soát bảo mật cơ bản đã hiện thực."),
        spacer(2),
        h2("12.2. Phân tích tấn công đã chống được"),
        rich(
            "**Khung tin phình to (length-prefix attack).** Kẻ tấn công gửi 4 byte độ dài là 4 GB rồi không gửi gì. "
            "Nếu bên nhận tin vào tiền tố độ dài và cấp phát ngay, một kết nối có thể làm cạn kiệt bộ nhớ. Trong "
            "thiết kế này, `recv_wire_message` kiểm tra `length > MAX_WIRE_MESSAGE_SIZE` và đóng kết nối **trước khi** "
            "cấp phát; bài `DeserializeRejectsOversizedLengthBeforeAllocating` khẳng định đúng hành vi đó."
        ),
        rich(
            "**Chỉ số ngoài phạm vi.** Một `REQUEST` với `index = 0xFFFFFFFF` có thể gây đọc ngoài mảng hoặc tràn số "
            "khi tính offset. Hệ thống so sánh với `num_chunks` trước khi đọc đĩa và đóng kết nối; bài "
            "`OutOfRangeChunkRequestClosesConnectionWithoutSlotLeak` còn kiểm tra rằng slot tải lên **không bị rò** "
            "sau khi đóng."
        ),
        rich(
            "**Mạo danh peer.** Tracker dùng chính `peer_id` do peer công bố nên một peer ác ý có thể announce cùng "
            "`peer_id` với peer khác và ghi đè IP/port của nó. Đây là điểm yếu đã biết: trong phạm vi đồ án, mô hình "
            "tin cậy là mạng lớp học và các peer đều do cùng nhóm triển khai, nên chưa có chữ ký số cho announce. "
            "Mục 16 ghi nhận hướng khắc phục (chữ ký trên announce bằng khóa công khai của peer)."
        ),
        h2("12.3. Điểm chưa đáp ứng"),
        rich(
            "Đề bài *khuyến khích* TLS/mã hóa thông điệp và yêu cầu xác thực người dùng. Đồ án chưa hiện thực TLS cho "
            "kênh HTTP của tracker, chưa có mã hóa kênh TCP giữa các peer, và dashboard Web không có xác thực. Đây là "
            "các khoảng trống được nêu thẳng thắn ở mục 16 thay vì được mô tả như đã có."
        ),
        rich(
            "**Một lỗ hổng đã sửa trong quá trình hoàn thiện.** Endpoint upload của Web dashboard nhận tham số "
            "`filename` từ query string và nối trực tiếp vào đường dẫn ghi tệp, cho phép ghi ra ngoài thư mục dữ liệu "
            "(`../`). Đây là lỗi path traversal cổ điển; bản nộp bài đã thêm bước kiểm tra chỉ cho phép tên tệp không "
            "chứa dấu phân cách đường dẫn trước khi ghi."
        ),
    ]


def section_13_implementation():
    return [
        h1("13. Hiện thực"),
        h2("13.1. Tổ chức mã nguồn"),
        table(
            ["Thư mục", "Số dòng", "Nội dung"],
            [
                ("`src/common/`", "946", "protocol, sha256, metadata, logger, net — thư viện tĩnh dùng chung"),
                ("`src/tracker/`", "475", "peer_registry, tracker_server, main"),
                ("`src/peer/`", "5 209", "peer_node, download_manager, upload_server, state_manager, database, file_manager, tracker_client, tui, web_ui"),
                ("`tests/`", "2 233", "11 tệp, 42 bài kiểm thử"),
                ("`scripts/`", "—", "demo_full, bench, inject_fault, start_peer, start_tracker, generate_test_file"),
            ],
            widths=[0.16, 0.12, 0.72],
        ),
        caption("Bảng 13.1 — Phân bố mã nguồn (8 863 dòng C++ trong `src/` và `tests/`)."),
        spacer(2),
        h2("13.2. Vòng đời một tiến trình peer"),
        numbered(
            [
                "Đọc tham số dòng lệnh; biến môi trường `TRACKER_URL`, `PEER_HOST` được dùng làm giá trị mặc định.",
                "Sinh Peer ID; mở `data_dir/peer.db`, tạo schema, khôi phục trạng thái torrent.",
                "Khởi động `UploadServer` (thread pool + accept loop) trên cổng đã cấp.",
                "Nếu `--seed`: sinh metadata, đăng ký mảnh với `UploadServer`, announce lên tracker.",
                "Nếu `--download`: nạp metadata, đăng ký tệp đích để có thể phục vụ mảnh ngay khi tải, rồi chạy "
                "`DownloadManager`.",
                "Luồng heartbeat announce lại bitfield mỗi 15 s.",
                "Chờ tín hiệu dừng hoặc (với `--exit-after-download`) kết thúc ngay khi tải xong.",
            ]
        ),
        code(
            """# Chạy một swarm tối thiểu trên một máy
./build/src/tracker/TrackerApp -p 8080 &
./build/src/peer/PeerApp --tracker http://127.0.0.1:8080 -p 0 \\
    --data-dir ./data/seed --seed ./ubuntu.iso --chunk-size 262144 &
./build/src/peer/PeerApp --tracker http://127.0.0.1:8080 -p 0 \\
    --data-dir ./data/leech --download ./ubuntu.iso.torrent.json \\
    --output-dir ./downloads"""
        ),
        h2("13.3. Ba giao diện người dùng"),
        rich(
            "**Dòng lệnh** là giao diện chính và là thứ mà các kịch bản demo dùng: mọi thao tác đều có tham số, log "
            "ghi ra stdout, và tiến trình có thể tắt bằng `SIGTERM`. **ncurses TUI** (`--tui`) hiển thị dashboard "
            "với danh sách torrent, thanh tiến trình, bảng peer và panel log; phím `p` tạm dừng, `r` tiếp tục, `q` "
            "thoát. **Web dashboard** (`--webui`) phục vụ một trang HTML tự chứa với lưới mảnh (piece map) cập nhật "
            "real-time, cho phép upload tệp để seed và upload `.torrent.json` để tải."
        ),
        rich(
            "Điểm thiết kế đáng chú ý: ba giao diện đều đọc cùng một `peer_node_status` và ba giao diện **không** sở "
            "hữu logic riêng. Logger có cơ chế `set_sink`, nên TUI và Web có thể hứng log mà không cần sửa lời gọi "
            "log ở bất kỳ đâu trong lõi."
        ),
        h2("13.4. Một số lựa chọn hiện thực"),
        rich_bullets(
            [
                "**Băm tệp theo luồng.** `SHA256::hash_file` dùng `EVP_DigestUpdate` trong vòng lặp khối, nên một "
                "tệp 100 GB không bao giờ bị nạp vào RAM. Bộ nhớ đỉnh đo được khi tải 100 MB chỉ khoảng 12 MB.",
                "**Ghi mảnh theo offset.** Tệp đích được tạo trước với kích thước chính xác, mỗi mảnh ghi thẳng vào "
                "offset `index × chunk_size`. Không cần tệp tạm cho từng mảnh, và ghép tệp là thao tác rỗng.",
                "**SQLite WAL.** `PRAGMA journal_mode=WAL` cho phép nhiều luồng đọc trong lúc một luồng ghi, kết hợp "
                "mutex ở lớp ứng dụng để tránh `database is locked`.",
                "**`std::atomic` cho cờ trạng thái.** Các cờ như `stop_flag`, `connected`, `choked` là atomic để "
                "đường nóng không phải lấy mutex.",
                "**Log thay thế được (sink).** Lõi không biết ai hiển thị log; CLI in ra stdout, TUI đưa vào panel, "
                "Web đưa vào bộ đệm cuộn.",
            ]
        ),
    ]


def section_14_testing():
    return [
        h1("14. Kiểm thử và kết quả"),
        h2("14.1. Bộ kiểm thử"),
        rich(
            "Đề bài yêu cầu tối thiểu 8 bài chức năng, 2 bài đồng thời, 2 bài lỗi–phục hồi và 2 bài bảo mật/dữ liệu "
            "sai, cộng một thí nghiệm hiệu năng. Bộ kiểm thử của đồ án vượt mức này ở mọi hạng mục:"
        ),
        table(
            ["Nhóm", "Tệp", "Số bài", "Nội dung chính"],
            [
                ("Chức năng", "test_sha256 (2), test_metadata (1), test_file_manager (1), test_protocol (6, gồm 2 NetTest), test_database (3), test_state_manager (1), test_peer_registry (5), test_tracker_server (1)", "20", "Băm, sinh/đọc metadata, đọc-ghi mảnh, roundtrip khung tin, CRUD SQLite, bitfield, registry, end-to-end tracker"),
                ("Đồng thời", "test_concurrent_download", "9", "2 seeder/1 leecher, 1 seeder/2 leecher, leecher phục vụ leecher, full-duplex, giới hạn slot, peer tham gia muộn, stress lặp, giao dịch cổng"),
                ("Lỗi và phục hồi", "test_fault_recovery", "4 (3 chạy, 1 chỉ Windows)", "seeder chết giữa chừng, chỉ có nguồn hỏng, resume sau kill"),
                ("Bảo mật / dữ liệu sai", "test_invalid_data", "10", "khung quá lớn, khung cụt, handshake sai, cổng ngoài phạm vi, chỉ số ngoài phạm vi, không treo"),
                ("Hiệu năng", "scripts/bench.sh", "9 cấu hình", "3 nguồn × 3 kích thước × 6 cấu hình đồng thời (mục 15)"),
            ],
            widths=[0.17, 0.34, 0.12, 0.37],
        ),
        caption("Bảng 14.1 — Bộ kiểm thử theo nhóm yêu cầu của đề bài."),
        spacer(2),
        h2("14.2. Kết quả chạy"),
        rich(
            "Chạy `ctest --output-on-failure` trên bản build Debug có ASan + UBSan: **42 bài được đăng ký, 41 bài "
            "chạy và đạt, 1 bài bị bỏ qua có chủ đích.** Bài bị bỏ qua là "
            "`RealProcessCrashResumesFromPersistedSqliteState`, chỉ biên dịch trên Windows vì cần kill tiến trình con "
            "theo cách Windows; trên Linux, cùng hành vi này được kiểm chứng bởi kịch bản demo 6 (mục 14.3). Toàn bộ "
            "chạy hoàn tất trong 7,63 giây."
        ),
        code(
            """$ ctest --output-on-failure
...
41/42 Test #41: FaultRecoveryTest.CorruptedOnlySourceIsRejectedAndDownloadFails  Passed  2.36 sec
42/42 Test #42: FaultRecoveryTest.RealProcessCrashResumesFromPersistedSqliteState  ***Skipped
100% tests passed out of 42
Total Test time (real) =   7.63 sec"""
        ),
        rich(
            "Một điểm đáng chú ý về **thời gian chạy test**: bộ test không chỉ kiểm tra hàm thuần mà còn khởi động "
            "tracker và peer thật trên cổng loopback, nên nó phát hiện được lỗi tích hợp mà unit test thuần bỏ sót. "
            "Ví dụ bài `PeerRegisteringAfterStartIsAdoptedPromptly` đặt ngưỡng 1 giây cho việc một peer xuất hiện "
            "giữa lúc tải phải được phát hiện; ngưỡng này đã bắt được một lỗi thật trong đó phiên đã thiết lập chỉ "
            "đọc bitfield lúc kết nối và không bao giờ thấy peer mới trở nên hữu ích."
        ),
        h2("14.3. Bảy kịch bản demo bắt buộc"),
        rich(
            "`scripts/demo_full.sh` chạy bảy kịch bản của đề bài trên các binary thật (bản Release), mỗi kịch bản "
            "dùng cổng và thư mục riêng, chỉ kill những PID nó tự tạo. Kết quả lần chạy được ghi lại nguyên văn:"
        ),
        table(
            ["#", "Kịch bản", "Bằng chứng đo được"],
            [
                ("1", "Đăng ký tracker và khám phá", "`/peers` trả về 2 peer cho đúng `file_id`"),
                ("2", "Tải một nguồn, xác minh SHA-256", "Mã băm tệp kết quả khớp nguồn"),
                ("3", "Tải đa nguồn", "2 peer cùng góp mảnh; mã băm khớp"),
                ("4", "Seeder chết, chuyển nguồn", "Kill seeder giữa chừng, phần còn lại lấy từ seeder sống, mã băm khớp"),
                ("5", "Phát hiện mảnh hỏng", "22 mảnh hỏng bị phát hiện và tải lại; mã băm khớp"),
                ("6", "Resume sau khi bị kill", "Kill ở mốc 10/129 mảnh; khởi động lại chỉ tải 111/129 mảnh; mã băm khớp"),
                ("7", "Tracker loại peer quá hạn", "Peer chết bị xóa khỏi `/peers` sau 50 s (ngưỡng 45 s, quét 10 s)"),
            ],
            widths=[0.05, 0.33, 0.62],
        ),
        caption("Bảng 14.2 — Kết quả bảy kịch bản demo; cả bảy đều PASS."),
        spacer(2),
        code(
            """$ ./scripts/demo_full.sh
================= demo results =================
DEMO 1  Tracker registration and discovery  PASS: /peers returned 2 peers ...
DEMO 2  Single-source download with SHA-256  PASS: sha256 6b3e7823... matches source
DEMO 3  Multi-source download                PASS: 2 peers contributed chunks ...
DEMO 4  Seeder failure with failover         PASS: killed pid ... mid-transfer ...
DEMO 5  Corrupted chunk detection            PASS: 22 corrupted chunk(s) detected ...
DEMO 6  Crash resume from persisted state    PASS: killed at 10/129 ..., refetched 111 of 129 ...
DEMO 7  Tracker expiry after 45s             PASS: stale peer dropped after 50s ...
================================================
ALL 7 DEMOS PASS"""
        ),
        rich(
            "Điểm quan trọng với hội đồng: các kịch bản không chỉ kiểm tra “chạy xong” mà kiểm tra **bằng chứng của "
            "cơ chế**. Kịch bản 3 thất bại nếu tệp đúng nhưng chỉ một peer góp mảnh; kịch bản 6 thất bại nếu tệp "
            "đúng nhưng khởi động lại tải lại toàn bộ (đó là tải mới, không phải resume); kịch bản 7 thất bại nếu "
            "thời gian loại ngoài khoảng hợp lý."
        ),
    ]


def section_15_performance():
    return [
        h1("15. Đánh giá hiệu năng"),
        h2("15.1. Phương pháp"),
        rich(
            "Môi trường đo: Intel Core i5-11300H (8 luồng), 15 GB RAM, EndeavourOS, GCC 16.1.1, bản **Release** "
            "(`-O3`, không sanitizer). Mỗi phép đo khởi động tiến trình peer từ đầu đến khi thoát và xác minh SHA-256 "
            "của tệp kết quả, nên con số bao gồm cả chi phí khởi động, đọc metadata, khám phá peer và ghi đĩa — đúng "
            "thứ người dùng cảm nhận."
        ),
        rich(
            "**Vì sao phải thêm độ trễ nhân tạo.** Trên loopback, đĩa và mạng cùng một máy nhanh hơn lõi mạng rất "
            "nhiều: tải 100 MB hoàn tất trong khoảng 0,5 giây và mọi cấu hình cho cùng một con số. Để đo được khả "
            "năng mở rộng, seeder được đặt `--chunk-delay-ms 50`, tức mỗi mảnh phục vụ chậm 50 ms — mô phỏng một "
            "đường truyền WAN. Với 400 mảnh của tệp 100 MB, một nguồn tốn lý thuyết 20 s; con số này cho phép kiểm "
            "tra chéo kết quả đo."
        ),
        h2("15.2. Khả năng mở rộng theo số nguồn"),
        table(
            ["Nguồn", "Kích thước", "Thời gian (s)", "Thông lượng (MB/s)", "Tăng tốc", "Bộ nhớ đỉnh (MB)"],
            [
                ("1", "100 MB", "20,53", "4,9", "1,00×", "12,2"),
                ("2", "100 MB", "10,54", "9,5", "1,95×", "12,9"),
                ("3", "100 MB", "7,05", "14,2", "2,91×", "13,8"),
            ],
            widths=[0.13, 0.16, 0.20, 0.22, 0.13, 0.16],
        ),
        caption("Bảng 15.1 — Tải 100 MB với 1, 2 và 3 seeder, mỗi seeder giới hạn 1 slot."),
        spacer(2),
        rich(
            "**Kết quả: gần tuyến tính.** Tăng tốc đo được là 1,95× với hai nguồn và 2,91× với ba nguồn — tức hiệu "
            "suất song song đạt 97 % lý thuyết. Đây là bằng chứng định lượng rằng mảnh được tải thật sự song song và "
            "rằng các seeder độc lập, không có khóa chung nào giữa đường tải của các peer khác nhau."
        ),
        h2("15.3. Chi phí cố định theo kích thước tệp"),
        table(
            ["Kích thước", "Số mảnh", "Thời gian (s)", "Thời gian lý thuyết", "Thông lượng (MB/s)"],
            [
                ("10 MB", "40", "2,51", "2,00", "4,0"),
                ("50 MB", "200", "10,55", "10,00", "4,7"),
                ("100 MB", "400", "20,55", "20,00", "4,9"),
            ],
            widths=[0.17, 0.13, 0.20, 0.25, 0.25],
        ),
        caption("Bảng 15.2 — Một nguồn, độ trễ 50 ms/mảnh; cột lý thuyết là num_chunks × 50 ms."),
        spacer(2),
        rich(
            "Hiệu số giữa thời gian đo và thời gian lý thuyết là **khoảng 0,5 s, không đổi** ở cả ba kích thước — đó "
            "chính là chi phí cố định của một phiên: khởi động tiến trình, sinh/đọc metadata, khám phá peer, bắt tay "
            "và xác minh ghi đĩa. Vì chi phí cố định không tăng theo kích thước tệp, tệp càng lớn thì thông lượng "
            "càng tiến gần giới hạn của đường truyền (4,0 → 4,7 → 4,9 MB/s). Đây là biểu hiện định lượng của việc "
            "chia mảnh: chi phí chia nhỏ chỉ trả một lần."
        ),
        h2("15.4. Đồng thời: nhiều leecher trên một swarm"),
        table(
            ["Cấu hình", "Tổng dữ liệu", "Leecher chậm nhất (s)", "Thông lượng tổng (MB/s)"],
            [
                ("1 leecher / 1 slot", "20 MB", "4,55", "4,4"),
                ("4 leecher / 1 slot", "80 MB", "4,56", "17,6"),
                ("8 leecher / 1 slot", "160 MB", "5,04", "31,7"),
                ("1 leecher / 8 slot", "20 MB", "4,55", "4,4"),
                ("4 leecher / 8 slot", "80 MB", "2,55", "31,4"),
                ("8 leecher / 8 slot", "160 MB", "2,04", "78,3"),
            ],
            widths=[0.27, 0.18, 0.28, 0.27],
        ),
        caption("Bảng 15.3 — Tệp 20 MB, seeder có 1 hoặc 8 slot tải lên."),
        spacer(2),
        rich(
            "Cột thời gian là leecher **chậm nhất** — thời gian thực để cả swarm nhận đủ tệp. Ba điều đáng chú ý:"
        ),
        rich_bullets(
            [
                "**Một slot không làm nghẽn swarm.** 8 leecher vẫn hoàn tất trong 5,04 s (so với 4,55 s cho một "
                "leecher), tức chậm hơn chỉ 11 %. Nguyên nhân đo được từ log: leecher tải xong mảnh nào thì phục vụ "
                "ngay mảnh đó cho leecher khác. Với 8 leecher, log của leecher cuối ghi **hai** peer đã góp mảnh, "
                "trong đó peer thứ hai là một leecher chứ không phải seeder. Đây là đặc tính cốt lõi của BitTorrent "
                "và là lý do mô hình P2P mở rộng được.",
                "**Tám slot cho thông lượng gấp 18×.** Cùng 8 leecher, seeder 8 slot đạt 78,3 MB/s so với 4,4 MB/s "
                "của một leecher — tức giới hạn slot là nút thắt thật sự khi seeder có băng thông.",
                "**Ai cũng phải trả giá khi chờ.** Với 1 slot, leecher thứ 8 phải chờ các leecher trước dùng xong "
                "slot, nhưng vì leecher-to-leecher bù đắp, cái giá chỉ là ~0,5 s.",
            ]
        ),
        rich(
            "Hạn chế của phép đo này: tất cả tiến trình nằm trên một máy nên chia sẻ cùng ổ đĩa và bộ nhớ đệm trang. "
            "Số liệu vì vậy thể hiện **hành vi của lõi phân tán**, không phải băng thông mạng thật; một phép đo WAN "
            "sẽ cần nhiều máy hoặc `tc netem`."
        ),
    ]


def section_16_limitations():
    return [
        h1("16. Hạn chế và hướng phát triển"),
        h2("16.1. Hạn chế đã biết"),
        table(
            ["Hạn chế", "Ảnh hưởng", "Hướng khắc phục"],
            [
                ("Tracker là điểm chết đơn", "Peer mới không khám phá được swarm khi tracker sập; các luồng đang chạy vẫn tiếp tục",
                 "Nhiều tracker + bầu chọn; DHT"),
                ("Peer ID không bền giữa các lần chạy", "Lịch sử peer không gắn với danh tính ổn định",
                 "Sinh và lưu Peer ID trong `data_dir` ở lần chạy đầu"),
                ("Không TLS, không mã hóa kênh", "Nội dung và announce có thể bị nghe lén trên mạng không tin cậy",
                 "TLS cho HTTP; handshake trao đổi khóa cho TCP"),
                ("Không xác thực announce", "Peer có thể mạo danh `peer_id` khác",
                 "Chữ ký số trên mỗi announce"),
                ("Web dashboard không có xác thực", "Bất kỳ ai truy cập được cổng dashboard đều upload/tải được",
                 "Token truy cập; bind loopback mặc định"),
                ("Không giới hạn băng thông (rate limiting)", "Một peer có thể chiếm toàn bộ slot",
                 "Token bucket theo peer; tit-for-tat"),
                ("Chưa có DHT, magnet link, mã hóa", "Phụ thuộc tracker và kênh ngoài để chia sẻ metadata",
                 "Đây là các tính năng nâng cao của đề bài"),
            ],
            widths=[0.25, 0.42, 0.33],
        ),
        caption("Bảng 16.1 — Các hạn chế được ghi nhận thẳng thắn."),
        spacer(2),
        h2("16.2. Hướng phát triển"),
        numbered(
            [
                "**Bền hóa danh tính** — lưu Peer ID trong thư mục dữ liệu để lịch sử peer có ý nghĩa qua nhiều lần chạy.",
                "**Nhiều tracker và failover tracker** — danh sách tracker cấu hình được; tự chuyển khi một tracker chết, "
                "đo thời gian chuyển đổi như một chỉ số mới.",
                "**TLS và xác thực** — TLS cho kênh tracker; chữ ký announce bằng khóa công khai của peer.",
                "**Tit-for-tat và giới hạn băng thông** — chọn peer dựa trên lịch sử đóng góp để chống ký sinh, đo "
                "ảnh hưởng tới thời gian tải.",
                "**Mô phỏng mạng thật** — dùng `tc netem` để thêm trễ, mất gói và giới hạn băng thông, thay cho "
                "`--chunk-delay-ms`.",
                "**Bền hóa thí nghiệm hiệu năng** — đo thêm độ trễ từng mảnh (p50/p95) và mức dùng CPU, không chỉ "
                "thời gian tổng.",
            ]
        ),
    ]


def section_17_conclusion():
    return [
        h1("17. Kết luận"),
        rich(
            "Đồ án đã xây dựng một hệ thống chia sẻ tệp P2P hoàn chỉnh theo mô hình BitTorrent: tracker chỉ quản lý "
            "siêu dữ liệu, peer tải và phục vụ mảnh song song, toàn bộ nội dung được xác minh bằng SHA-256 từng mảnh "
            "và toàn tệp. Hệ thống gồm các tiến trình độc lập giao tiếp qua socket, không dùng bộ nhớ chung hay tệp "
            "chia sẻ làm kênh giao tiếp, và chạy được cả trên Linux lẫn Windows."
        ),
        rich(
            "Ba nhóm kết quả chính. **Thứ nhất, tính đúng đắn:** 41 bài kiểm thử tự động và bảy kịch bản demo bắt buộc "
            "đều đạt, bao gồm các tình huống khó như seeder chết giữa chừng, mảnh hỏng, và tiến trình bị kill giữa "
            "lúc tải. **Thứ hai, khả năng mở rộng:** thời gian tải giảm gần tuyến tính theo số nguồn (1,95× với hai "
            "nguồn, 2,91× với ba nguồn), và một seeder chỉ có một slot vẫn phục vụ tám leecher trong thời gian gần như "
            "không đổi nhờ leecher-to-leecher — đặc tính mở rộng đặc trưng của P2P. **Thứ ba, chi phí:** bộ nhớ đỉnh "
            "khoảng 12–14 MB cho tệp 100 MB, chi phí cố định của một phiên khoảng 0,5 s."
        ),
        rich(
            "Bài học lớn nhất về hệ phân tán là cách tách hai loại nhất quán: chấp nhận nhất quán cuối cùng cho thông "
            "tin “ai giữ gì” nhưng giữ nhất quán mạnh cho nội dung. Cách tách này loại bỏ nhu cầu khóa phân tán và "
            "đồng hồ logic, đồng thời vẫn cho kết quả đúng tuyệt đối về dữ liệu. Bài học thứ hai là các tham số "
            "timeout có quan hệ với nhau: `choke_wait_ms` phải nhỏ hơn `io_timeout_ms` của phía tải, nếu không một "
            "seeder khỏe nhưng đang bận sẽ bị coi là chết — một lỗi chỉ lộ ra khi chạy tải đồng thời ở quy mô lớn."
        ),
    ]


def section_18_references():
    return [
        h1("18. Tài liệu tham khảo"),
        numbered(
            [
                "B. Cohen, *Incentives Build Robustness in BitTorrent Systems*, Workshop on Economics of Peer-to-Peer "
                "Systems, 2003. — Cơ chế choke/unchoke, tit-for-tat và lý do chia mảnh.",
                "BitTorrent Protocol Specification v1.0 (BEP 3), BitTorrent Enhancement Proposals. — Cấu trúc "
                "metainfo và giao thức trao đổi mảnh.",
                "CrowCpp, *Crow — A Fast and Easy to Use C++ Microframework for Web*, v1.3.4. — Máy chủ HTTP của "
                "tracker và dashboard.",
                "nlohmann, *JSON for Modern C++*, v3.x. — Tuần tự hóa siêu dữ liệu và announce.",
                "SQLite Documentation, *Write-Ahead Logging*. — Chế độ WAL cho truy cập đồng thời.",
                "OpenSSL Documentation, *EVP Message Digests*. — API băm SHA-256 theo luồng.",
                "GoogleTest Documentation. — Khung kiểm thử đơn vị và tích hợp.",
                "Docker Documentation, *Multi-stage builds* và *Compose file reference*. — Đóng gói và triển khai.",
                "A. S. Tanenbaum, M. van Steen, *Distributed Systems*, 4th edition. — Nhất quán, chịu lỗi, mô hình lỗi.",
                "M. Kleppmann, *Designing Data-Intensive Applications*, O'Reilly, 2017. — Đồng thời, nhất quán, "
                "idempotency và retry.",
            ]
        ),
        rich(
            "Toàn bộ thư viện bên thứ ba được dùng đúng như giấy phép của nó (Crow: BSD-3, nlohmann/json: MIT, "
            "GoogleTest: BSD-3, SQLite: public domain, OpenSSL: Apache-2.0). Phần lõi phân tán — tracker, registry, "
            "giao thức, chọn mảnh, retry, phục hồi — do nhóm tự viết."
        ),
    ]


def section_19_appendix():
    return [
        h1("19. Phụ lục"),
        h2("19.1. Lệnh tái lập toàn bộ kết quả"),
        code(
            """# Build bản Release (đo hiệu năng) và bản Debug (ASan, chạy test)
cmake -S . -B build         -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build         -j"$(nproc)"
cmake --build build-release -j"$(nproc)"

# Toàn bộ 42 bài kiểm thử
ctest --test-dir build --output-on-failure

# Bảy kịch bản demo bắt buộc (bản Debug hoặc Release đều được)
./scripts/demo_full.sh

# Thí nghiệm hiệu năng (đọc QBT_BUILD_DIR, mặc định build-release)
BENCH_CSV=docs/bench/results.csv ./scripts/bench.sh"""
        ),
        h2("19.2. Tham số cấu hình"),
        table(
            ["Tham số", "Mặc định", "Ý nghĩa"],
            [
                ("`--port` / `-p`", "6881 (peer), 8080 (tracker)", "Cổng lắng nghe; `0` để OS cấp"),
                ("`--tracker`", "`$TRACKER_URL` hoặc `http://localhost:8080`", "URL tracker"),
                ("`--chunk-size`", "262144 (256 KB)", "Kích thước mảnh khi seed/tải"),
                ("`--max-downloads`", "5", "Số nguồn tải song song tối đa"),
                ("`--max-uploads`", "5", "Số slot phục vụ tối đa"),
                ("`--pipeline-depth`", "3", "Số REQUEST chưa trả lời mỗi kết nối"),
                ("`--io-timeout-ms`", "4000", "Timeout socket (ms)"),
                ("`--chunk-delay-ms`", "0", "Độ trễ nhân tạo mỗi mảnh — dùng cho demo và benchmark"),
                ("`--corrupt-chunks`", "rỗng", "Danh sách chỉ số mảnh cố tình hỏng — dùng cho kịch bản 5"),
                ("`--announce-ip`", "tự dò / `$PEER_HOST`", "Địa chỉ công bố với tracker"),
                ("`--webui` / `--webui-port`", "tắt / 8081", "Bật dashboard HTTP"),
                ("`--exit-after-download`", "tắt", "Thoát ngay khi tải xong — dùng cho script"),
            ],
            widths=[0.26, 0.30, 0.44],
        ),
        caption("Bảng 19.1 — Các tham số dòng lệnh chính của PeerApp."),
        spacer(2),
        h2("19.3. Số liệu thô của thí nghiệm hiệu năng"),
        rich(
            "Số liệu thô nằm trong `docs/bench/results.csv` và được sinh trực tiếp bởi `scripts/bench.sh`; mọi bảng ở "
            "mục 15 đều đọc từ tệp này, không nhập tay. In lại nguyên văn để người đọc có thể đối chiếu:"
        ),
        code(
            """dimension,configuration,bytes,elapsed_ms,contributors,peak_rss_kb,verified
sources,1-source,104857600,20531,1,12460,yes
sources,2-source,104857600,10537,2,13192,yes
sources,3-source,104857600,7054,3,14172,yes
filesize,10MB,10485760,2514,1,12104,yes
filesize,50MB,52428800,10548,1,12236,yes
filesize,100MB,104857600,20548,1,12292,yes
concurrency,1-leecher/1-slot,20971520,4546,-,0,yes
concurrency,4-leecher/1-slot,83886080,4556,-,0,yes
concurrency,8-leecher/1-slot,167772160,5043,-,0,yes
concurrency,1-leecher/8-slot,20971520,4553,-,0,yes
concurrency,4-leecher/8-slot,83886080,2549,-,0,yes
concurrency,8-leecher/8-slot,167772160,2044,-,0,yes"""
        ),
        rich(
            "Cột `bytes` là tổng dữ liệu cả swarm nhận được ở phần đồng thời (kích thước tệp × số leecher), nên cột "
            "thông lượng ở mục 15.4 là **tổng hợp**, không phải của từng leecher. `peak_rss_kb = 0` nghĩa là phép đo "
            "theo dõi tiến trình đó không lấy được mẫu (tiến trình con của script), và các bảng tương ứng hiển thị "
            "`-` thay vì bịa một con số."
        ),
        h2("19.4. Lịch sử phát triển"),
        table(
            ["Mốc", "Nội dung chính đã hoàn tất"],
            [
                ("M0–M2", "Khung CMake ba thư viện; SHA-256, metadata, wire protocol, SQLite CRUD + unit test"),
                ("M3", "Tracker HTTP (3 endpoint), peer registry thread-safe, tracker client, dọn peer quá hạn"),
                ("M4", "Upload server đa luồng, download manager với retry, state manager + SQLite, CLI"),
                ("M5", "Tải song song đa nguồn, rarest-first, choke/unchoke và giới hạn slot, 9 bài concurrency"),
                ("M6", "7 kịch bản demo tự động, TUI ncurses, blacklist theo mảnh, 10 bài dữ liệu sai"),
                ("M7", "Docker hóa, ASan + profiling, Web dashboard, CI Linux + Windows, báo cáo và slide"),
            ],
            widths=[0.12, 0.88],
        ),
        caption("Bảng 19.2 — Các cột mốc đã hoàn tất, đối chiếu với lịch sử commit và Issue trên GitHub."),
    ]
