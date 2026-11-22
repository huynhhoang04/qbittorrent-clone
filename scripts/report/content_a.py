"""Report body, part A: front matter through section 10.

Sections follow the 19-item outline in the course specification, §10.2.
Every number in here was measured on this repository (see docs/bench/results.csv
and the demo run in §14), not estimated.
"""

from reportlab.lib.units import mm
from reportlab.platypus import NextPageTemplate, PageBreak, Spacer

from report_lib import (
    SANS,
    SANS_BOLD,
    ACCENT,
    ACCENT_LIGHT,
    GREY,
    ReportDoc,
    architecture_diagram,
    bullets,
    caption,
    code,
    frame_diagram,
    h1,
    h2,
    h3,
    numbered,
    p,
    rich,
    rich_bullets,
    sequence_diagram,
    spacer,
    table,
)

# --------------------------------------------------------------------- metadata
COURSE = "Hệ thống phân tán (Distributed Systems)"
TOPIC = "DS02 — Hệ thống chia sẻ tệp P2P mô phỏng BitTorrent"
TITLE_RUNNING = "Báo cáo kỹ thuật — P2P File-Sharing System (DS02)"

MEMBERS = [
    # github login, display name, student id, main responsibility, evidence
    ("huynhhoang04", "Em Huynh", "Điền MSSV",
     "Tracker service (registry, HTTP API, client), M4 peer networking "
     "(upload server, download manager, state manager, CLI), Web UI, CI + demo hardening",
     "Issues #1–#8, #21; PR #25, #26"),
    ("giaback", "giaback", "Điền MSSV",
     "M5 tải song song đa nguồn, connection limiting / choking, full-duplex, "
     "concurrency tests; M6 fault tolerance, demo automation, ncurses TUI",
     "Issues #9–#14; PR #19, #20"),
    ("TruongNguyen2703", "Nguyễn Hữu Trường", "Điền MSSV",
     "Container hóa (Docker/Compose), sửa crash tracker, ASan + profiling + code cleanup, "
     "README / .env.example / final test / release tag",
     "Issues #15, #16, #18; PR #22, #23, #24"),
]


def front_matter(doc):
    """Cover page and table of contents."""
    from reportlab.platypus import Paragraph

    story = [NextPageTemplate("body")]
    story.append(Spacer(1, 42 * mm))
    story.append(Paragraph("BÁO CÁO KỸ THUẬT", doc.styles_cover_sub))
    story.append(Spacer(1, 6 * mm))
    story.append(Paragraph("Hệ thống chia sẻ tệp P2P<br/>mô phỏng BitTorrent", doc.styles_cover_title))
    story.append(Spacer(1, 4 * mm))
    story.append(
        Paragraph(
            "Chia nhỏ tệp thành mảnh · tải song song đa nguồn · kiểm tra toàn vẹn SHA-256 · "
            "chịu lỗi và phục hồi sau sự cố",
            doc.styles_cover_sub,
        )
    )
    story.append(Spacer(1, 26 * mm))
    story.append(
        Paragraph(
            "<b>Môn học:</b> %s<br/>"
            "<b>Mã đề tài:</b> DS02<br/>"
            "<b>Nhóm:</b> 03 thành viên<br/>"
            "<b>Học viện Công nghệ Bưu chính Viễn thông (PTIT)</b>" % COURSE,
            doc.styles_cover_meta,
        )
    )
    story.append(Spacer(1, 18 * mm))
    rows = [(m[1], m[2], m[0]) for m in MEMBERS]
    story.append(
        table(
            ["Thành viên", "MSSV", "GitHub"],
            rows,
            widths=[0.44, 0.28, 0.28],
            align="CENTER",
        )
    )
    story.append(Spacer(1, 14 * mm))
    story.append(
        Paragraph(
            "Học kỳ 1 · Năm học 2026–2027<br/>"
            "Hạn nộp: 22/11/2026 · Demo và vấn đáp: 23/11/2026 và 30/11/2026",
            doc.styles_cover_sub,
        )
    )

    story.append(PageBreak())
    story.append(Paragraph("Mục lục", doc.styles_toc_title))
    story.append(doc.toc)
    story.append(PageBreak())
    return story


def section_2_task_assignment(doc):
    """2. Group information and task assignment."""
    story = [
        h1("2. Thông tin nhóm và phân công công việc"),
        rich(
            "Nhóm gồm ba thành viên, mỗi người phụ trách một nhóm thành phần riêng và "
            "đều tham gia kiểm thử chéo. Bảng dưới đây được đối chiếu trực tiếp từ lịch sử "
            "GitHub của kho mã nguồn: mỗi mốc công việc đều có Issue được giao và Pull Request "
            "tương ứng đã được trộn vào nhánh `main`."
        ),
        spacer(2),
        table(
            ["Thành viên", "Phụ trách chính", "Minh chứng trong kho"],
            [(m[1], m[3], m[4]) for m in MEMBERS],
            widths=[0.17, 0.53, 0.30],
        ),
        caption("Bảng 2.1 — Phân công công việc và minh chứng trên GitHub."),
        rich(
            "**Nguyên tắc phối hợp.** Nhánh `main` chỉ nhận thay đổi qua Pull Request có ít nhất "
            "một thành viên khác đọc lại; mỗi PR gắn với một Issue theo mốc M3–M7. Điều này tạo ra "
            "lịch sử commit có thể truy vết cho từng thành viên, đáp ứng yêu cầu “each member must be "
            "responsible for a specific part” của đề bài."
        ),
        rich(
            "**Cách tính đóng góp.** Ở thời điểm nộp bài, kho có 34 commit trên `main`: 17 commit của "
            "`giaback`, 14 commit của `huynhhoang04` (8 dưới tên `huynhhoang04`, 6 dưới tên cấu hình "
            "`Em huynh`), 3 commit của `TruongQuynh` (tài khoản `TruongNguyen2703`). Số commit không "
            "phản ánh đầy đủ khối lượng: các PR #19 và #20 của `giaback` mang phần lớn logic tải song song "
            "và chịu lỗi, còn các PR #22–#24 của `TruongNguyen2703` tập trung vào hạ tầng và độ ổn định."
        ),
        spacer(3),
        h2("2.1. Phân chia theo mốc"),
        table(
            ["Mốc", "Nội dung", "Người phụ trách", "Trạng thái"],
            [
                ("M0–M2", "Khung dự án, SHA-256, metadata, wire protocol, SQLite CRUD", "Cả nhóm", "Hoàn thành"),
                ("M3", "Tracker service: peer registry, HTTP API, tracker client", "huynhhoang04", "Hoàn thành"),
                ("M4", "Peer networking: upload server, download manager, state manager, CLI", "huynhhoang04", "Hoàn thành"),
                ("M5", "Tải song song đa nguồn, connection limiting, full-duplex, concurrency tests", "giaback", "Hoàn thành"),
                ("M6", "Chịu lỗi, 7 kịch bản demo tự động, giao diện ncurses", "giaback", "Hoàn thành"),
                ("M7", "Docker, profiling, dọn mã nguồn, README, .env.example, release tag", "TruongNguyen2703", "Hoàn thành"),
                ("M7", "Báo cáo kỹ thuật và slide thuyết trình", "Cả nhóm", "Hoàn thành"),
            ],
            widths=[0.09, 0.53, 0.20, 0.18],
        ),
        caption("Bảng 2.2 — Phân công theo cột mốc của kế hoạch dự án."),
    ]
    return story


def section_3_abstract():
    return [
        h1("3. Tóm tắt (Abstract)"),
        rich(
            "Báo cáo trình bày một hệ thống chia sẻ tệp ngang hàng (peer-to-peer) hoạt động theo mô hình "
            "BitTorrent, được cài đặt từ đầu bằng C++17 trên Linux và Windows. Hệ thống gồm hai loại tiến "
            "trình độc lập: **TrackerApp** — một máy chủ HTTP chỉ quản lý siêu dữ liệu và danh sách peer, "
            "tuyệt đối không giữ hay chuyển tiếp nội dung tệp; và **PeerApp** — tiến trình lai vừa là máy chủ "
            "TCP phục vụ mảnh dữ liệu, vừa là trình tải đa luồng, vừa là giao diện người dùng "
            "(dòng lệnh, ncurses TUI hoặc Web dashboard)."
        ),
        rich(
            "Tệp được chia thành các mảnh (chunk) kích thước cố định, mỗi mảnh được băm SHA-256 độc lập và "
            "toàn bộ tệp có thêm một mã băm tổng. Siêu dữ liệu `.torrent.json` đóng vai trò “hợp đồng” giữa "
            "người chia sẻ và người tải: nó cho phép xác minh từng mảnh ngay khi nhận và xác minh lần cuối "
            "sau khi ghép. Định danh tệp chính là mã băm SHA-256 của toàn bộ nội dung, nhờ đó hai tệp trùng "
            "nội dung luôn trùng định danh mà không cần cơ chế đặt tên tập trung."
        ),
        rich(
            "Về mặt hệ thống phân tán, đồ án hiện thực hóa sáu nhóm vấn đề: định danh và khám phá peer qua "
            "tracker; đồng thời và đồng bộ hóa với thread pool, mutex và SQLite ở chế độ WAL; nhất quán cuối "
            "cùng cho bitfield kèm kiểm tra toàn vẹn theo từng mảnh; chịu lỗi với timeout, retry có giới hạn, "
            "danh sách đen theo từng mảnh và phục hồi trạng thái sau khi tiến trình bị kill; trao đổi mảnh "
            "trùng lặp / sai thứ tự nhờ chỉ số mảnh và tính lũy đẳng; và các kiểm soát đầu vào cơ bản."
        ),
        rich(
            "Kết quả đo trên một máy 8 lõi: 42 bài kiểm thử tự động (41 chạy, 1 bài chỉ dành cho Windows bị bỏ "
            "qua có chủ đích) và 7/7 kịch bản demo bắt buộc đều đạt. Trong thí nghiệm hiệu năng có kiểm soát, "
            "thời gian tải giảm gần tuyến tính theo số nguồn: 20,53 s với 1 nguồn, 10,54 s với 2 nguồn và "
            "7,05 s với 3 nguồn cho cùng một tệp 100 MB; bộ nhớ đỉnh giữ ổn định khoảng 12–14 MB."
        ),
        rich(
            "**Từ khóa:** peer-to-peer, BitTorrent, tracker, chia nhỏ dữ liệu, tải song song, SHA-256, "
            "nhất quán cuối cùng, chịu lỗi, SQLite WAL, C++17."
        ),
    ]


def section_4_requirements():
    return [
        h1("4. Mô tả bài toán và yêu cầu"),
        h2("4.1. Bài toán"),
        rich(
            "Cần xây dựng một hệ thống cho phép nhiều máy tính trao đổi tệp trực tiếp với nhau thay vì qua "
            "một máy chủ trung tâm. Yêu cầu cốt lõi của mô hình BitTorrent là: người gửi không phải phục vụ "
            "toàn bộ tệp cho từng người nhận, và người nhận không phải đợi tải xong mới có ích. Một peer vừa "
            "tải xong mảnh nào thì phải có khả năng chia sẻ ngay mảnh đó cho peer khác."
        ),
        rich(
            "Máy chủ trung tâm (tracker) chỉ được phép trả lời câu hỏi “ai đang giữ tệp có mã định danh X”. "
            "Đề bài loại trực tiếp phương án tracker phục vụ nội dung tệp; đây cũng là điểm khác biệt then chốt "
            "giữa một hệ thống P2P thực sự và một hệ thống tải tệp tập trung đội lốt P2P."
        ),
        h2("4.2. Yêu cầu chức năng"),
        *bullets(
            [
                "**Siêu dữ liệu:** mã định danh tệp, tên, kích thước, kích thước mảnh, số mảnh, mã băm từng "
                "mảnh, mã băm toàn tệp và thông tin tracker.",
                "**Chia sẻ và khám phá:** seeder chia tệp thành mảnh, đăng ký với tracker; tracker quản lý "
                "danh sách peer giữ tệp; peer công bố danh sách mảnh mà mình đang giữ.",
                "**Tải tệp:** tải từ ít nhất hai peer, tải nhiều mảnh song song, vừa tải vừa chia sẻ, yêu cầu "
                "lại mảnh lỗi từ peer khác, giới hạn số kết nối.",
                "**Toàn vẹn và phục hồi:** xác minh mã băm từng mảnh, từ chối mảnh sai, ghép đúng thứ tự, "
                "xác minh mã băm toàn tệp, tiếp tục tải khi một nguồn rời mạng, tiếp tục tải sau khi khởi "
                "động lại chương trình.",
            ]
        ),
        h2("4.3. Yêu cầu phi chức năng"),
        table(
            ["Nhóm yêu cầu", "Nội dung", "Cách hệ thống đáp ứng"],
            [
                ("Phân tán", "≥ 3 tiến trình độc lập, giao tiếp qua mạng",
                 "1 TrackerApp + N PeerApp; HTTP cho tracker, TCP thô cho dữ liệu"),
                ("Định danh", "Mỗi thành phần có định danh duy nhất",
                 "Peer ID duy nhất theo tiến trình; File ID = SHA-256 nội dung"),
                ("Đồng thời", "Xử lý nhiều kết nối/yêu cầu cùng lúc",
                 "Thread pool ở cả hai phía; mutex + condition_variable bảo vệ trạng thái"),
                ("Ghi log", "Log nêu node, thời điểm, loại sự kiện, kết quả",
                 "Logger dùng chung, có mức INFO/SUCCESS/WARN/ERROR"),
                ("Cấu hình", "Không hardcode bí mật; cấu hình qua môi trường",
                 "`.env.example`; `TRACKER_URL`, `PEER_HOST` đọc từ biến môi trường"),
                ("Triển khai", "Chạy được trên nhiều máy/VM/container",
                 "Dockerfile cho từng thành phần + `docker-compose.yml` 4 node"),
            ],
            widths=[0.17, 0.38, 0.45],
        ),
        caption("Bảng 4.1 — Yêu cầu phi chức năng và cách đáp ứng."),
        h2("4.4. Phạm vi"),
        rich(
            "Đồ án tập trung vào lõi phân tán: tracker, trao đổi mảnh, đồng thời, chịu lỗi và phục hồi. "
            "Các tính năng nâng cao của BitTorrent thật (DHT, magnet link, mã hóa kênh, thuật toán "
            "tit-for-tat, giới hạn băng thông) không nằm trong phạm vi bắt buộc và được ghi nhận ở mục 16."
        ),
    ]


def section_5_issues():
    return [
        h1("5. Các vấn đề của hệ thống phân tán"),
        rich(
            "Mục này phân tích sáu vấn đề phân tán mà đồ án phải giải quyết, mỗi vấn đề nêu biểu hiện cụ thể "
            "trong hệ thống và cơ chế đã chọn. Các mục 9–12 trình bày chi tiết thiết kế và kết quả kiểm chứng "
            "tương ứng."
        ),
        table(
            ["Vấn đề", "Biểu hiện trong hệ thống", "Cơ chế giải quyết"],
            [
                ("Định danh và khám phá", "Peer mới tham gia cần biết ai đang giữ tệp; một peer chết phải "
                 "biến mất khỏi danh sách",
                 "Tracker HTTP tập trung; announce định kỳ 15 s; dọn peer quá hạn 45 s"),
                ("Đồng thời và đồng bộ hóa", "Nhiều mảnh được yêu cầu song song; peer vừa gửi vừa nhận; "
                 "bitfield và SQLite bị nhiều luồng cùng truy cập",
                 "Thread pool, `std::mutex`, `condition_variable`, SQLite WAL, giới hạn slot"),
                ("Nhất quán", "Tracker và các peer không thể biết ngay một peer vừa có thêm mảnh",
                 "Nhất quán cuối cùng cho bitfield; mỗi mảnh tự xác minh SHA-256"),
                ("Chịu lỗi và phục hồi", "Peer rời mạng giữa chừng, dữ liệu hỏng, tiến trình bị kill",
                 "Timeout, retry có giới hạn, blacklist theo mảnh, checkpoint SQLite, resume"),
                ("Trùng lặp và sai thứ tự", "Cùng một mảnh có thể về từ hai peer; các mảnh về không đúng thứ tự",
                 "Chỉ số mảnh tuyệt đối, ghi lũy đẳng, theo dõi mảnh đang bay (in-flight)"),
                ("Bảo mật cơ bản", "Peer độc hại gửi dữ liệu giả, khung tin quá lớn, chỉ số ngoài phạm vi",
                 "Giới hạn kích thước khung, kiểm tra bàn giao, chặn chỉ số ngoài phạm vi, prepared statement"),
            ],
            widths=[0.19, 0.40, 0.41],
        ),
        caption("Bảng 5.1 — Sáu vấn đề phân tán và cơ chế giải quyết."),
        spacer(2),
        rich(
            "**Điểm đáng chú ý.** Trong hệ thống này, “nhất quán” không thể là nhất quán mạnh: không có cơ chế "
            "khóa phân tán nào giữa các peer, và tracker chỉ là bộ nhớ tạm. Thay vào đó hệ thống chọn **nhất "
            "quán cuối cùng** (eventual consistency) cho bitfield — thông tin “ai giữ mảnh nào” lan truyền theo "
            "chu kỳ announce — nhưng **nhất quán mạnh cho nội dung**: một mảnh hoặc khớp SHA-256 và được ghi "
            "nhận, hoặc bị loại. Cách tách hai loại nhất quán này là quyết định thiết kế quan trọng nhất của đồ án."
        ),
    ]


def section_6_analysis():
    return [
        h1("6. Phân tích và lựa chọn giải pháp"),
        h2("6.1. Các phương án đã cân nhắc"),
        table(
            ["Quyết định", "Phương án A", "Phương án B", "Lựa chọn và lý do"],
            [
                ("Khám phá peer", "Tracker tập trung (HTTP)",
                 "DHT phi tập trung",
                 "**A.** DHT vượt xa phạm vi 10 tuần và khó kiểm chứng; tracker tập trung cho phép "
                 "quan sát toàn bộ trạng thái mạng khi demo, và đề bài DS02 cho phép rõ ràng"),
                ("Kênh dữ liệu", "TCP thô + khung tin nhị phân",
                 "HTTP cho từng mảnh",
                 "**A.** HTTP thêm phần đầu gói tin cho mỗi mảnh 256 KB và không giữ được kết nối lâu; "
                 "TCP thô cho phép pipeline nhiều REQUEST trên một kết nối"),
                ("Kích thước mảnh", "256 KB cố định", "Theo tham số dòng lệnh",
                 "**Cả hai.** Mặc định 256 KB, cho phép `--chunk-size` để đo ảnh hưởng của chi phí cố định"),
                ("Lưu trạng thái peer", "SQLite (WAL)", "File JSON",
                 "**A.** Cần cập nhật trạng thái từng mảnh với tần suất cao và đồng thời; WAL cho phép "
                 "đọc/ghi song song mà không khóa toàn bộ tệp"),
                ("Chọn mảnh", "Tuần tự", "Random-first + rarest-first",
                 "**B.** Random-first giúp peer sớm có dữ liệu để chia sẻ; rarest-first phân tán dữ liệu "
                 "hiếm và tăng khả năng sống sót của swarm"),
                ("Đồng bộ bitfield", "Nhất quán mạnh (khóa phân tán)", "Nhất quán cuối cùng",
                 "**B.** Khóa phân tán giữa các peer là chi phí lớn không cần thiết; xác minh SHA-256 "
                 "đã bảo đảm tính đúng đắn của nội dung"),
            ],
            widths=[0.14, 0.22, 0.17, 0.47],
        ),
        caption("Bảng 6.1 — Các quyết định thiết kế chính."),
        spacer(2),
        h2("6.2. Vì sao tracker không được giữ nội dung tệp"),
        rich(
            "Nếu tracker phục vụ mảnh dữ liệu, hệ thống suy biến thành mô hình máy chủ tệp tập trung: băng thông "
            "của tracker trở thành nút thắt và là điểm chết duy nhất. Trong thiết kế này, tracker chỉ trả lời hai "
            "câu hỏi — “peer này giữ tệp X với bitfield nào” và “những ai giữ tệp X” — nên nó có thể phục vụ hàng "
            "trăm peer bằng vài chục byte mỗi chu kỳ. Hệ quả là một tracker có thể chết mà các luồng đang tải "
            "giữa các peer vẫn tiếp tục, đúng như kịch bản chịu lỗi ở mục 11."
        ),
        h2("6.3. Vì sao chọn tải song song theo mảnh"),
        rich(
            "Tải tuần tự một tệp 100 MB từ một nguồn bị chặn bởi độ trễ khứ hồi và tốc độ của nguồn đó. Chia "
            "thành mảnh cho phép ba lợi ích đồng thời: (i) nhiều nguồn cùng phục vụ một tệp; (ii) mảnh lỗi chỉ "
            "cần tải lại một mảnh; (iii) một peer vừa tải xong mảnh đầu tiên đã có thể phục vụ mảnh đó cho peer "
            "khác. Thí nghiệm ở mục 15 định lượng lợi ích thứ nhất; bài kiểm thử "
            "`PartialPeersServeEachOtherWhileDownloading` chứng minh lợi ích thứ ba bằng cách cho hai peer, mỗi "
            "peer chỉ có một nửa số mảnh, trao đổi cho nhau cho đến khi cả hai hoàn tất."
        ),
        h2("6.4. Mô hình lỗi"),
        rich(
            "Hệ thống giả định mô hình lỗi **crash-stop** kết hợp **mất kết nối tạm thời**: một peer có thể biến "
            "mất bất cứ lúc nào mà không thông báo, có thể gửi dữ liệu hỏng, và mạng có thể trễ. Hệ thống không "
            "giả định lỗi Byzantine phức tạp (peer nói dối một cách tinh vi) ngoài phạm vi dữ liệu sai lệch, vì "
            "mã băm SHA-256 đã đủ để phát hiện mọi sai lệch nội dung."
        ),
    ]


def section_7_architecture():
    return [
        h1("7. Kiến trúc hệ thống"),
        h2("7.1. Tổng quan"),
        rich(
            "Hệ thống gồm hai loại tiến trình độc lập, không dùng bộ nhớ chung và không đọc chung tệp trạng thái: "
            "**TrackerApp** và **PeerApp**. Mọi giao tiếp đều qua socket. Tracker chạy một máy chủ HTTP; peer chạy "
            "một máy chủ TCP phục vụ mảnh và một hoặc nhiều giao diện người dùng."
        ),
        architecture_diagram(430, 148),
        caption("Hình 7.1 — Kiến trúc vận hành: tracker chỉ quản lý siêu dữ liệu, mảnh dữ liệu đi trực tiếp giữa các peer."),
        rich(
            "Đường nét đứt là luồng điều khiển (announce/khám phá) qua HTTP. Đường nét liền là luồng dữ liệu qua "
            "TCP thô. Không có đường nào từ tracker tới nội dung tệp — đây là ràng buộc kiến trúc được kiểm chứng "
            "bằng kịch bản demo 1 và 3."
        ),
        h2("7.2. Thành phần và trách nhiệm"),
        rich(
            "Bảng 7.1 chia thành phần theo tiến trình sở hữu. `CommonLib` là thư viện tĩnh mà cả hai tiến trình liên "
            "kết vào, nên nó không phải một tiến trình thứ ba; ba tiến trình độc lập của hệ thống là một TrackerApp và "
            "các PeerApp (tối thiểu hai để tạo thành một swarm)."
        ),
        table(
            ["Thành phần", "Sở hữu", "Trách nhiệm chính"],
            [
                ("TrackerServer", "TrackerApp", "Định tuyến HTTP `/announce`, `/peers`, `/health`; kiểm tra dữ liệu vào"),
                ("PeerRegistry", "TrackerApp", "Ánh xạ file_id → danh sách peer; thread-safe; dọn peer quá hạn"),
                ("PeerNode", "PeerApp", "Điều phối vòng đời: khởi tạo, heartbeat, seed, download, dừng"),
                ("UploadServer", "PeerApp", "Thread pool phục vụ REQUEST/PIECE; choke/unchoke; giới hạn slot"),
                ("DownloadManager", "PeerApp", "Chọn mảnh, pipeline, xác minh SHA-256, blacklist, retry"),
                ("StateManager", "PeerApp", "Bitfield trong RAM + đồng bộ SQLite; nạp lại khi khởi động"),
                ("FileManager", "PeerApp", "Đọc/ghi mảnh theo offset; tạo tệp đích; ghép tệp"),
                ("TrackerClient", "PeerApp", "HTTP client tối giản: announce, get_peers, health_check"),
                ("Database", "PeerApp", "Bọc SQLite3, WAL, prepared statement, mutex ở lớp ứng dụng"),
                ("Tui / WebUi", "PeerApp", "ncurses dashboard; HTTP dashboard + thao tác seed/download"),
            ],
            widths=[0.21, 0.13, 0.66],
        ),
        caption("Bảng 7.1 — Thành phần của hai tiến trình (trích các thành phần chính)."),
        table(
            ["Thành phần", "Sở hữu", "Trách nhiệm chính"],
            [
                ("Metadata", "CommonLib", "Sinh và đọc `.torrent.json`; băm từng mảnh và toàn tệp"),
                ("SHA256", "CommonLib", "Bọc OpenSSL EVP; băm buffer và băm tệp theo luồng"),
                ("Protocol", "CommonLib", "Khung tin nhị phân: serialize/deserialize, giới hạn kích thước"),
                ("Logger", "CommonLib", "Log thread-safe, có mức độ và sink thay thế được (CLI/TUI/Web)"),
                ("net", "CommonLib", "Bọc socket POSIX/Winsock: timeout, resolve IPv4, dò IP ra ngoài"),
            ],
            widths=[0.21, 0.13, 0.66],
        ),
        caption("Bảng 7.2 — Thư viện dùng chung `CommonLib`, liên kết vào cả hai tiến trình."),
        spacer(2),
        h2("7.3. Triển khai"),
        rich(
            "Kho mã nguồn cung cấp hai Dockerfile nhiều tầng (builder → runtime) và một `docker-compose.yml` dựng "
            "một mạng bridge riêng với 4 node: 1 tracker và 3 peer có hostname riêng (`tracker`, `peer-seeder`, "
            "`peer-leecher-1`, `peer-leecher-2`). Mỗi peer dùng một volume riêng nên trạng thái SQLite độc lập. "
            "Cách này cho phép mô phỏng nhiều nút mạng trên một máy, đúng như đề bài cho phép khi không có nhiều "
            "kết nối Internet thật."
        ),
        code(
            """# deploy/docker-compose.yml (rút gọn)
services:
  tracker:            # 8080:8080, network aliases: tracker
  peer-seeder:        # TRACKER_URL=http://tracker:8080, PEER_HOST=peer-seeder
  peer-leecher-1:     # TRACKER_URL=http://tracker:8080, PEER_HOST=peer-leecher-1
  peer-leecher-2:     # TRACKER_URL=http://tracker:8080, PEER_HOST=peer-leecher-2
networks:
  p2p_network: { driver: bridge }"""
        ),
        rich(
            "Biến `PEER_HOST` là phần bổ sung quan trọng cho môi trường container: bên trong mạng bridge, địa chỉ "
            "IP mà tracker quan sát được là IP nội bộ của container, nhưng peer cần công bố hostname mà các "
            "container khác phân giải được. Peer đọc `PEER_HOST` trước, sau đó mới tự dò IP ra ngoài."
        ),
    ]


def section_8_components():
    return [
        h1("8. Thiết kế thành phần và giao thức"),
        h2("8.1. Siêu dữ liệu `.torrent.json`"),
        rich(
            "Siêu dữ liệu là một tệp JSON do seeder sinh ra và được chia sẻ ngoài kênh P2P (qua chat, HTTP "
            "dashboard hoặc copy tay). Nó chứa mọi thứ cần để xác minh nội dung mà **không** chứa nội dung:"
        ),
        code(
            """{
  "file_name":   "ubuntu.iso",
  "file_size":   104857600,
  "chunk_size":  262144,
  "num_chunks":  400,
  "file_hash":   "9f2c...<SHA-256 của toàn bộ tệp>",
  "tracker_url": "http://tracker:8080",
  "chunk_hashs": ["a71b...", "5c02...", "... 400 phần tử"]
}"""
        ),
        rich(
            "**Định danh tệp (File ID) chính là `file_hash`** — mã băm SHA-256 của toàn bộ nội dung. Lựa chọn này "
            "mang lại ba tính chất: định danh duy nhất mà không cần cấp phát, hai tệp cùng nội dung tự động cùng "
            "định danh (chống trùng lặp), và bất kỳ ai cầm siêu dữ liệu đều xác minh được cả từng mảnh lẫn toàn tệp. "
            "`chunk_hashs` có đúng `num_chunks` phần tử, phần tử cuối có thể ngắn hơn `chunk_size` nếu kích thước tệp "
            "không chia hết."
        ),
        h2("8.2. Giao thức Peer ↔ Tracker (HTTP + JSON)"),
        table(
            ["Phương thức", "Đường dẫn", "Nội dung", "Phản hồi"],
            [
                ("POST", "/announce", "`{peer_id, ip, port, file_id, bitfield[]}`", "`{status:\"ok\"}` hoặc 400"),
                ("GET", "/peers?file_id=…", "—", "`[{peer_id, ip, port, bitfield[]}]`"),
                ("GET", "/health", "—", "`{status:\"ok\", total_peers:N}`"),
            ],
            widths=[0.14, 0.22, 0.36, 0.28],
        ),
        caption("Bảng 8.1 — Ba endpoint của tracker."),
        spacer(2),
        rich(
            "Tracker kiểm tra nghiêm ngặt trường vào: `peer_id` và `file_id` phải là chuỗi, `bitfield` phải là mảng "
            "boolean, `port` phải nằm trong khoảng 1–65535. Khi một peer announce lại cùng `file_id` với cùng "
            "`peer_id`, bản ghi được cập nhật (không nhân đôi). Địa chỉ IP được lấy theo thứ tự ưu tiên: "
            "`X-Forwarded-For` → `X-Real-IP` → địa chỉ kết nối, và tracker từ chối các giá trị cục bộ "
            "(`127.0.0.1`, `0.0.0.0`) khi phía gọi là mạng ngoài, nhằm tránh công bố địa chỉ không kết nối được."
        ),
        h2("8.3. Giao thức Peer ↔ Peer (TCP thô, khung tin nhị phân)"),
        rich(
            "TCP là một dòng byte liên tục, không phải chuỗi thông điệp: một lần `recv()` có thể trả về nửa khung "
            "hoặc hai khung dính liền. Vì vậy mọi khung đều có tiền tố độ dài và việc đọc dùng `recv_exact` lặp "
            "đến khi đủ số byte, gửi dùng `send_exact`. Mọi số nguyên nhiều byte đều ở **network byte order** "
            "(`htonl`/`ntohs`)."
        ),
        frame_diagram(430, 58),
        caption("Hình 8.1 — Cấu trúc khung tin: 4 byte độ dài, 1 byte mã loại, payload."),
        table(
            ["Mã", "Loại", "Payload", "Ý nghĩa"],
            [
                ("0x00", "HANDSHAKE", "`uint16 len · peer_id · file_id`", "Bắt tay, khai báo danh tính và tệp"),
                ("0x01", "BITFIELD", "1 byte/mảnh (0/1)", "Peer gửi biết những mảnh mình đang giữ"),
                ("0x02", "REQUEST", "`uint32 index`", "Yêu cầu một mảnh"),
                ("0x03", "PIECE", "`uint32 index · data`", "Trả nội dung mảnh"),
                ("0x04", "CANCEL", "`uint32 index`", "Hủy yêu cầu (pipeline đầy)"),
                ("0x05", "KEEPALIVE", "rỗng", "Giữ kết nối sống"),
                ("0x06", "INTERESTED", "rỗng", "Xin một slot tải lên"),
                ("0x07", "NOT_INTERESTED", "rỗng", "Trả lại slot"),
                ("0x08", "CHOKE", "rỗng", "Từ chối phục vụ"),
                ("0x09", "UNCHOKE", "rỗng", "Đồng ý phục vụ"),
            ],
            widths=[0.08, 0.20, 0.32, 0.40],
        ),
        caption("Bảng 8.2 — Mười loại khung của giao thức peer-to-peer."),
        spacer(2),
        rich(
            "Giới hạn cứng: khung tối đa **8 MB** (`MAX_WIRE_MESSAGE_SIZE`), `peer_id` và `file_id` tối đa **128 byte**. "
            "Khung vượt giới hạn bị đóng kết nối **trước khi** cấp phát bộ nhớ, nên một peer độc hại không thể ép "
            "đối phương cấp phát 4 GB chỉ bằng một tiền tố độ dài."
        ),
        h2("8.4. Trình tự một phiên tải"),
        sequence_diagram(430, 185),
        caption("Hình 8.2 — Trình tự bắt tay và trao đổi mảnh giữa leecher, tracker và seeder."),
        rich(
            "Sau khi nhận `BITFIELD`, leecher gửi `INTERESTED`; seeder cấp một slot và trả `UNCHOKE`. Leecher sau đó "
            "gửi tối đa `pipeline_depth` yêu cầu `REQUEST` chưa được trả lời cùng lúc, và bù thêm mỗi khi nhận một "
            "`PIECE`. Nếu seeder hết slot trong `choke_wait_ms`, nó trả `CHOKE` và leecher chuyển sang peer khác — "
            "đây là cơ chế cân bằng tải tự nhiên, không cần bộ điều phối trung tâm."
        ),
    ]


def section_9_discovery():
    return [
        h1("9. Định danh và khám phá"),
        h2("9.1. Định danh peer"),
        rich(
            "Mỗi tiến trình peer sinh một Peer ID duy nhất khi khởi động, có dạng `peer-<port>-<16 byte hex>`, từ "
            "ba nguồn ngẫu nhiên ghép lại (`random_device`, đồng hồ steady, PID). Việc ghép PID là cần thiết vì "
            "trong demo nhiều peer cùng chạy trên một máy và cùng xin cổng `0` (OS cấp cổng bất kỳ): nếu chỉ dựa "
            "vào cổng yêu cầu, hai peer sẽ trùng định danh, và `DownloadManager` — vốn bỏ qua peer trùng ID của "
            "chính mình — sẽ không thấy phần còn lại của swarm."
        ),
        rich(
            "Peer ID **không** được lưu bền: mỗi lần chạy là một danh tính mới. Đây là lựa chọn có chủ đích cho phạm "
            "vi đồ án, và nó an toàn về mặt chức năng vì tracker dọn peer quá hạn sau 45 s; đổi lại, lịch sử peer "
            "trong bảng `peer_history` không gắn được với một danh tính ổn định giữa các lần chạy. Mục 16 ghi nhận "
            "đây là hướng cải tiến."
        ),
        h2("9.2. Định danh tệp"),
        rich(
            "File ID = SHA-256 của toàn bộ nội dung, dài 64 ký tự hex. Mọi bảng trong SQLite và mọi khóa trên "
            "tracker đều dùng chuỗi này. Nhờ tính chất của hàm băm, hai peer độc lập cùng chia sẻ một tệp sẽ tự "
            "động hội tụ vào cùng một swarm mà không cần đăng ký tên."
        ),
        h2("9.3. Đăng ký và khám phá"),
        numbered(
            [
                "Seeder sinh `.torrent.json`; băm từng mảnh theo luồng, không nạp cả tệp vào RAM.",
                "Seeder khởi động `UploadServer` trên cổng của nó, ghi trạng thái `verified` cho toàn bộ mảnh.",
                "Seeder announce lên tracker: `file_id`, `bitfield` toàn `true`, `ip`, `port`.",
                "Leecher nhận `.torrent.json` ngoài kênh P2P, gọi `GET /peers?file_id=…` và nhận danh sách peer.",
                "Leecher kết nối TCP tới từng peer, bắt tay, nhận `BITFIELD` thực tế của peer đó.",
            ]
        ),
        h2("9.4. Heartbeat và dọn peer quá hạn"),
        rich(
            "Peer gửi announce lại mỗi **15 giây** (vòng lặp ngủ 30 × 500 ms). Tracker chạy một luồng dọn riêng, "
            "thức mỗi **10 giây** và xóa mọi bản ghi có `last_seen` cũ hơn **45 giây**. Hai chu kỳ này được chọn "
            "sao cho một peer chết được phát hiện sau tối đa 45 + 10 = 55 giây trong trường hợp xấu nhất, trong khi "
            "một lần announce bị trễ không làm peer bị loại oan. Kịch bản demo 7 đo được thời gian loại thực tế là "
            "**50 giây**."
        ),
        rich(
            "Cơ chế này là ví dụ điển hình của đánh đổi trong hệ phân tán: chu kỳ càng ngắn thì phát hiện lỗi càng "
            "nhanh nhưng tải lên tracker càng lớn. Với 15 s/45 s, một tracker nhỏ có thể phục vụ hàng trăm peer bằng "
            "băng thông không đáng kể, và các peer đang tải giữa nhau không hề bị ảnh hưởng khi tracker chậm."
        ),
    ]


def section_10_concurrency():
    return [
        h1("10. Đồng thời, đồng bộ hóa và nhất quán"),
        h2("10.1. Các xung đột có thể xảy ra"),
        table(
            ["Xung đột", "Hậu quả nếu không xử lý", "Cơ chế"],
            [
                ("Hai luồng cùng nhận một mảnh", "Tải trùng, tốn băng thông, ghi đè tệp", "Tập `claimed_chunks`; nhận mảnh không nằm trong in-flight bị coi là lỗi giao thức"),
                ("Bitfield đọc/ghi đồng thời", "Race condition, mảnh bị đánh dấu sai", "`std::mutex` trong `StateManager`"),
                ("SQLite bị nhiều luồng truy cập", "Khóa dài, lỗi `database is locked`", "WAL + mutex cấp ứng dụng + prepared statement"),
                ("Quá nhiều kết nối tới một seeder", "Cạn kiệt tài nguyên, mọi peer cùng chậm", "Giới hạn slot tải lên, `CHOKE`, hàng đợi có giới hạn"),
                ("Tracker đọc/ghi registry đồng thời", "Bản ghi mất, vector bị hỏng", "Một mutex cho toàn bộ thao tác registry"),
                ("Mảnh được yêu cầu lại khi đang bay", "Hai luồng cùng ghi một offset", "In-flight tracking; chỉ requeue sau timeout"),
            ],
            widths=[0.26, 0.32, 0.42],
        ),
        caption("Bảng 10.1 — Các xung đột và cơ chế xử lý."),
        spacer(2),
        h2("10.2. Mô hình nhất quán"),
        rich(
            "Hệ thống áp dụng **nhất quán cuối cùng** cho metadata của swarm: tracker và các peer khác chỉ biết một "
            "peer vừa có thêm mảnh sau chu kỳ announce kế tiếp (tối đa 15 s). Ngược lại, với **nội dung**, hệ thống "
            "áp dụng quy tắc mạnh hơn: một mảnh chỉ được ghi nhận khi mã băm khớp, và toàn tệp chỉ được coi là hoàn "
            "tất khi mã băm tổng khớp. Nói cách khác, trạng thái “ai giữ gì” có thể trễ, nhưng trạng thái “tôi giữ "
            "gì” luôn đúng."
        ),
        rich(
            "Cách diễn đạt chính xác: bitfield là **CRDT không cần hợp nhất** ở mức mỗi peer — mỗi peer tự sở hữu "
            "bitfield của mình và chỉ công bố nó; không có hai bên nào cùng ghi một bitfield. Nhờ vậy không cần "
            "vector clock hay Lamport clock: không tồn tại tình huống hai bản ghi xung đột cần phân xử."
        ),
        h2("10.3. Chọn mảnh: random-first rồi rarest-first"),
        rich(
            "`DownloadManager` chọn mảnh theo hai giai đoạn. Với mảnh đầu tiên của một phiên tải, nó chọn **ngẫu "
            "nhiên đều** trong số mảnh còn thiếu, để peer sớm có dữ liệu xác minh và trở nên hữu ích cho swarm. Từ "
            "mảnh thứ hai trở đi, nó tính **độ phổ biến** của từng mảnh bằng cách đếm số phiên đang có mảnh đó, chọn "
            "nhóm mảnh hiếm nhất, rồi chọn ngẫu nhiên trong nhóm đó để nhiều peer không cùng nhắm một mảnh."
        ),
        code(
            """// download_manager.cpp — chọn mảnh hiếm nhất
unordered_map<int, int> availability;
for (int index : candidates) {
    int count = 0;
    for (const auto& entry : sessions)
        if (entry.second->bitfield[index]) count++;
    availability[index] = count;
}
int rarest = numeric_limits<int>::max();
for (const auto& entry : availability) rarest = min(rarest, entry.second);
// chọn ngẫu nhiên đều trong các mảnh có availability == rarest"""
        ),
        rich(
            "Phân tích: nếu tất cả peer cùng chọn mảnh phổ biến nhất, swarm sẽ hội tụ về một tập mảnh hẹp và mảnh "
            "hiếm chỉ được tải khi gần xong — đúng lúc nguồn duy nhất giữ mảnh đó có thể đã rời mạng. Rarest-first "
            "đảo ngược điều đó: nó chủ động “vá” các lỗ hổng hiếm trước, tăng xác suất hoàn tất khi có churn."
        ),
        h2("10.4. Pipeline và giới hạn tài nguyên"),
        rich(
            "Mỗi phiên giữ tối đa `pipeline_depth` yêu cầu chưa được trả lời (mặc định 3) — đủ để che độ trễ khứ "
            "hồi mà không dồn quá nhiều mảnh vào một kết nối. Số phiên song song bị giới hạn bởi "
            "`max_active_downloads` (mặc định 5), và phía phục vụ giới hạn `max_active_uploads` (mặc định 5) kèm "
            "hàng đợi 64 kết nối. Vượt hàng đợi, kết nối bị đóng ngay thay vì làm chậm cả seeder."
        ),
        rich(
            "**Chống đói (starvation).** Seeder giữ slot tối đa `choke_wait_ms` = 2000 ms cho một client xin slot; "
            "nếu không có, nó trả `CHOKE`. Giá trị này phải nhỏ hơn `io_timeout_ms` của phía tải (4000 ms), nếu "
            "không một seeder khỏe nhưng đang bận sẽ bị phía tải coi là đã chết và tiêu tốn ngân sách lỗi — chi "
            "tiết này được ghi chú ngay trong mã nguồn và là một bài học thực tế khi tinh chỉnh timeout."
        ),
    ]
