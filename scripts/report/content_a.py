"""Report body: cover and sections 2–10; results load from recorded evidence."""

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
    peer_flow_diagram,
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
    ("huynhhoang04", "Hoàng Văn Huynh", "B23DCDT131",
     "Tracker service (registry, HTTP API, client), M4 peer networking "
     "(upload server, download manager, state manager, CLI), Web UI, CI + demo hardening",
     "a1207bc, 81c7aa1, 6a06265; tích hợp Web UI qua PR #25"),
    ("giaback", "Nguyễn Gia Bách", "B23DCVT042",
     "M5 tải song song đa nguồn, connection limiting / choking, full-duplex, "
     "concurrency tests; M6 fault tolerance, demo automation, ncurses TUI",
     "2b05515, fafe5bb, c45a9db, dfdc6fc; PR #19, #21"),
    ("TruongNguyen2703", "Nguyễn Hữu Trường", "B23DCVT424",
     "Container hóa (Docker/Compose), sửa crash tracker, ASan + profiling + code cleanup, "
     "README / .env.example / final test / release tag",
     "a94a8f5, 012e362, 5e06734; PR #22–#24"),
]


def front_matter(doc):
    from reportlab.platypus import Paragraph
    return [NextPageTemplate("body"), Spacer(1, 1), PageBreak(),
            Paragraph("Mục lục", doc.styles_toc_title), doc.toc, PageBreak()]



def section_2_task_assignment(doc):
    return [h1("2. Thông tin nhóm và phân công công việc"),
        rich("Nhóm 11 thực hiện đề tài DS02 thuộc học phần **Hệ thống phân tán**, lớp **INT1405_CLC-20261-01**. Nhóm gồm **Hoàng Văn Huynh — B23DCDT131**, **Nguyễn Gia Bách — B23DCVT042** và **Nguyễn Hữu Trường — B23DCVT424**."),
        table(["Thành viên / GitHub","Phần việc chính","Minh chứng đóng góp"],
              [(m[1]+" / "+m[0],m[3],m[4]) for m in MEMBERS],widths=[.24,.46,.30]),
        caption("Bảng 2.1 — Phân công công việc và commit/PR tương ứng trong kho mã nguồn [2]."),
        h2("2.1. Tổ chức công việc"),
        rich("Công việc được chia theo ranh giới thành phần: tracker và tích hợp hệ thống; cơ chế trao đổi mảnh, tải song song và phục hồi; triển khai và ổn định môi trường chạy. Các thành phần dùng chung cấu trúc metadata, định danh tệp, khung thông điệp và quy tắc kiểm tra SHA-256 để kết nối thành một hệ thống thống nhất."),
        rich("Các thay đổi được quản lý bằng Git, Issue và Pull Request. Kết quả tích hợp được đánh giá qua kiểm thử chức năng, đồng thời, lỗi–phục hồi, dữ liệu không hợp lệ và thí nghiệm hiệu năng. Mỗi thành viên cần hiểu kiến trúc chung và giải thích được cơ chế thuộc phần việc của mình theo yêu cầu của học phần [1]."),
    ]




def section_3_abstract():
    return [h1("3. Tóm tắt (Abstract)"),
        rich("Đồ án xây dựng nguyên mẫu chia sẻ tệp ngang hàng cho đề tài DS02 bằng C++17. `TrackerApp` quản lý ánh xạ giữa mã tệp và các peer đang giữ mảnh; `PeerApp` vừa tải mảnh qua TCP, vừa phục vụ các mảnh đã xác minh cho peer khác. Bố trí tối thiểu theo đề bài là **1 tracker và 3 peer độc lập**. Tracker không lưu hoặc chuyển tiếp nội dung tệp."),
        rich("Metadata JSON chứa kích thước, danh sách SHA-256 từng mảnh và SHA-256 toàn tệp. Scheduler chia việc giữa các kết nối, chọn mảnh đầu ngẫu nhiên rồi ưu tiên mảnh hiếm theo thông tin đã biết. Mutex bảo vệ trạng thái nội bộ; SQLite lưu tiến độ cục bộ để phục hồi. Timeout, retry hữu hạn và loại nguồn theo từng mảnh xử lý trường hợp mất nguồn hoặc dữ liệu sai."),
        rich('Kết quả đánh giá ngày 22/11/2026 ghi nhận **48 bài được đăng ký: 47 PASS và 1 SKIP trên Linux** ở cả bản Release và Debug có ASan/UBSan. Bảy kịch bản tự động đạt; các kiểm tra HTTP bổ sung xác minh đường dẫn và tính ổn định của dịch vụ. CSV hiệu năng lưu 12 cấu hình có kiểm tra hash; với tệp 100 MiB và delay 50 ms/mảnh, thời gian giảm khi tăng số nguồn. Số liệu cụ thể ở mục 15 được lấy trực tiếp từ CSV; đây là phép đo loopback một lần cho mỗi cấu hình, chưa phải kết luận về WAN.'),
        rich("Báo cáo chỉ rõ các giới hạn: tracker đơn, peer ID chưa bền qua lần chạy, metadata cần được tin cậy, chưa xác thực danh tính và phân quyền đầy đủ, chưa mã hóa kênh. Vì vậy, nguyên mẫu thể hiện các cơ chế lõi DS02 nhưng chưa thể được tuyên bố đáp ứng trọn vẹn yêu cầu bảo mật §4.5 [1]."),
        rich("**Từ khóa:** P2P, tracker, chunk, tải song song đa nguồn, SHA-256, eventual consistency, retry, checkpoint, SQLite, C++17."),
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
                 "Tối thiểu 1 TrackerApp + 3 PeerApp; HTTP điều khiển, TCP dữ liệu"),
                ("Định danh", "Mỗi thành phần có định danh duy nhất",
                 "Peer ID duy nhất theo tiến trình; File ID = SHA-256 nội dung"),
                ("Đồng thời", "Xử lý nhiều kết nối/yêu cầu cùng lúc",
                 "Thread pool ở cả hai phía; mutex + condition_variable bảo vệ trạng thái"),
                ("Ghi log", "Log nêu node, thời điểm, loại sự kiện, kết quả",
                 "Log theo tiến trình; timestamp và mức sự kiện; đối chiếu Peer ID qua dòng khởi động"),
                ("Cấu hình", "Không hardcode bí mật; cấu hình qua môi trường",
                 "Đọc .env và các biến môi trường; CLI được ưu tiên cao nhất"),
                ("Triển khai", "Chạy được trên nhiều máy/VM/container",
                 "Dockerfile cho từng thành phần + `docker-compose.yml` 4 node"),
            ],
            widths=[0.17, 0.38, 0.45],
        ),
        caption("Bảng 4.1 — Yêu cầu phi chức năng và cách đáp ứng."),
        h2("4.4. Ma trận đối chiếu đề bài"),
        table(["Điều khoản [1]", "Thiết kế / bằng chứng", "Đánh giá"], [
            ("§4.1; §6.2", "1 tracker + ít nhất 3 peer; socket; thư mục riêng; mục 7", "Kiến trúc tối thiểu 4 tiến trình"),
            ("§4.2; §6.3", "HTTP discovery, TCP framing, định danh, bitfield; mục 8–9", "Đã có trong nguồn"),
            ("§4.3", "Claim mảnh có khóa, full-duplex, nhiều leecher; mục 10, 14", "Có test đồng thời đã PASS"),
            ("§4.4; §6.4", "Mất nguồn; mảnh hỏng; checkpoint/resume; mục 11, 14", "Có test lỗi; demo LAN cần lưu log"),
            ("§4.5", "Giới hạn khung, kiểm tra dữ liệu và prepared statement; mục 12", "Đáp ứng một phần; thiếu xác thực/ACL"),
            ("§9", "26 chức năng/cấu hình + 9 đồng thời + 3 lỗi + 10 dữ liệu sai; 12 cấu hình đo", "47 PASS, 1 SKIP ở mỗi cấu hình"),
            ("§10–11", "Báo cáo 19 mục; README; demo; Git; tag/release", "Phân biệt hiện vật đã có và checklist nộp"),
        ], widths=[.18,.55,.27]),
        caption("Bảng 4.2 — Truy vết yêu cầu, minh chứng và phần còn giới hạn."),
        h2("4.5. Phạm vi"),
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
                 "Giới hạn kích thước khung, kiểm tra bắt tay, chặn chỉ số ngoài phạm vi, prepared statement"),
            ],
            widths=[0.19, 0.40, 0.41],
        ),
        caption("Bảng 5.1 — Sáu vấn đề phân tán và cơ chế giải quyết."),
        spacer(2),
        rich(
            '**Hai thuộc tính khác nhau.** Trạng thái khám phá “ai giữ mảnh nào” được cập nhật qua announce và có thể trễ; thiết kế chấp nhận nhất quán cuối cùng khi các cập nhật được truyền thành công. Nội dung là dữ liệu bất biến, được kiểm tra hash theo metadata tin cậy. Kiểm tra toàn vẹn không phải mô hình nhất quán mạnh và không cần đồng thuận để giải quyết chỉnh sửa cạnh tranh. Mutex/SQLite giải quyết xung đột trong một peer; không dùng chúng làm kênh chia sẻ trạng thái giữa các tiến trình.'
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
                 "**A.** DHT tăng độ phức tạp khám phá và kiểm chứng ở quy mô đồ án; tracker tập trung cho phép "
                 "quan sát toàn bộ trạng thái mạng khi demo, và đề bài DS02 cho phép rõ ràng"),
                ("Kênh dữ liệu", "TCP thô + khung tin nhị phân",
                 "HTTP cho từng mảnh",
                 "**A.** HTTP có thể hỗ trợ keep-alive nhưng cần thiết kế API/truyền mảnh riêng; "
                 "TCP thô cho phép pipeline nhiều REQUEST trên một kết nối"),
                ("Kích thước mảnh", "256 KiB cố định", "Theo tham số dòng lệnh",
                 "**Cả hai.** Mặc định 256 KiB, cho phép `--chunk-size` để đo ảnh hưởng của chi phí cố định"),
                ("Lưu trạng thái peer", "SQLite (WAL)", "File JSON",
                 "**A.** Cần cập nhật trạng thái từng mảnh với tần suất cao và đồng thời; WAL cho phép "
                 "reader và writer cùng tiến triển; vẫn chỉ có một writer SQLite tại một thời điểm"),
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
            "câu hỏi — “peer này giữ tệp X với bitfield nào” và “những ai giữ tệp X” — nên nó có thể phục vụ "
            "nhiều peer mà không gánh lưu lượng nội dung. Payload bitfield vẫn tăng theo số mảnh, và tải truy vấn tăng theo số peer. Khi tracker dừng, các kết nối dữ liệu đã biết có thể tiếp tục "
            "giữa các peer nếu còn đủ nguồn; khám phá peer mới sẽ bị ảnh hưởng (mục 11)."
        ),
        h2("6.3. Vì sao chọn tải song song theo mảnh"),
        rich(
            "Tải tuần tự một tệp 100 MiB từ một nguồn bị chặn bởi độ trễ khứ hồi và tốc độ của nguồn đó. Chia "
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
            "SHA-256 hỗ trợ phát hiện sai lệch với metadata tin cậy; nó không xác thực người gửi và không chống việc thay cả metadata lẫn tệp."
        ),
    ]


def section_7_architecture():
    return [h1("7. Kiến trúc hệ thống"),
        h2("7.1. Mô hình P2P có tracker"),
        rich("Hệ thống sử dụng mô hình **P2P có tracker tập trung**: tracker hỗ trợ các peer tìm thấy nhau, còn peer trực tiếp trao đổi mảnh dữ liệu. Kiến trúc gồm hai loại chương trình độc lập là TrackerApp và PeerApp. Một cấu hình tối thiểu theo DS02 có **1 TrackerApp và ít nhất 3 PeerApp**, tương ứng ít nhất bốn tiến trình giao tiếp qua mạng [1]."),
        rich("Tracker là điểm tập trung của thông tin khám phá, không phải máy chủ phân phối tệp. Một peer không cố định là bên gửi hoặc bên nhận: nó là leecher khi còn thiếu mảnh và trở thành seeder khi giữ đủ tệp; trong cả hai trạng thái, nó có thể phục vụ những mảnh đã xác minh. Do đó khả năng cung cấp dữ liệu của swarm tăng khi người tải bắt đầu sở hữu mảnh hợp lệ."),
        architecture_diagram(460,165),
        caption("Hình 7.1 — Kiến trúc logic: tracker phục vụ khám phá qua HTTP; dữ liệu mảnh truyền trực tiếp qua TCP giữa các peer."),
        rich("Kiến trúc tách **luồng điều khiển** và **luồng dữ liệu**. Luồng điều khiển gồm announce/heartbeat và tra cứu danh sách peer qua HTTP/JSON. Luồng dữ liệu gồm HANDSHAKE, BITFIELD, REQUEST và PIECE qua kết nối TCP trực tiếp. Đường nét đứt trong hình biểu diễn trao đổi điều khiển; đường liền biểu diễn trao đổi mảnh. Tracker không nhận, lưu hoặc chuyển tiếp nội dung mảnh."),
        h2("7.2. Thành phần tracker"),
        rich("TrackerApp chứa TrackerServer và PeerRegistry. TrackerServer tiếp nhận yêu cầu HTTP, kiểm tra định dạng và chuyển thao tác hợp lệ tới registry. PeerRegistry lưu ánh xạ **File ID → danh sách peer**, trong đó mỗi bản ghi có Peer ID, endpoint IP/port, bitfield và last_seen. Đây là danh mục vị trí dữ liệu; không phải bản sao đầy đủ của tệp metadata chứa các hash mảnh."),
        table(["Thao tác","Trách nhiệm","Kết quả"],[
            ("POST /announce","Thêm hoặc cập nhật bản ghi theo cặp File ID–Peer ID; làm mới thời điểm hoạt động","Peer được công bố cùng endpoint và các mảnh nó tự khai đang giữ"),
            ("GET /peers?file_id=…","Lấy snapshot các peer đang đăng ký cho tệp","Danh sách địa chỉ và bitfield để bên tải chọn nguồn"),
            ("GET /health","Trả trạng thái dịch vụ và số bản ghi peer","Hỗ trợ kiểm tra khả năng truy cập tracker"),
            ("Dọn bản ghi quá hạn","Định kỳ loại peer có last_seen quá ngưỡng lease","Giảm danh sách nguồn không còn khả dụng"),
        ],widths=[.25,.43,.32]),
        caption("Bảng 7.1 — Chức năng của tracker và ranh giới dữ liệu được quản lý."),
        rich("Registry được lưu trong RAM và bảo vệ bằng mutex để yêu cầu announce, truy vấn và dọn peer không làm hỏng trạng thái khi chạy đồng thời. Peer heartbeat khoảng 15 giây; tracker quét khoảng 10 giây và loại bản ghi cũ hơn 45 giây. Thông tin trả về có thể trễ so với thực tế: bên tải vẫn phải thử kết nối và kiểm tra bitfield trong phiên TCP, không coi một địa chỉ từ tracker là bảo đảm nguồn còn sống."),
        h2("7.3. Thành phần bên trong một peer"),
        rich("PeerApp được điều phối bởi PeerNode. PeerNode tạo các thành phần lưu trữ và mạng, khởi động dịch vụ upload, quản lý phiên tải, duy trì heartbeat và cung cấp snapshot cho giao diện. Các mô-đun dưới đây nằm trong **cùng một tiến trình peer**; chúng không được tính thành các node phân tán riêng."),
        table(["Mô-đun","Trách nhiệm chính","Dữ liệu / tài nguyên quản lý"],[
            ("TrackerClient","Announce và tìm nguồn qua HTTP","Endpoint tracker, danh sách peer theo File ID"),
            ("DownloadManager","Chọn mảnh, phân công worker, pipeline yêu cầu, kiểm tra hash và retry","Pending/claimed/in-flight; các session tải; ngân sách lỗi"),
            ("UploadServer","Nhận kết nối TCP, bắt tay và trả mảnh đã có; giới hạn slot","Socket lắng nghe, hàng đợi kết nối, thread pool, slot upload"),
            ("StateManager","Theo dõi bitfield, mảnh verified/missing và tiến độ","Trạng thái trong RAM; cập nhật tiến độ xuống DB cục bộ"),
            ("FileManager","Tạo tệp đích, đọc và ghi từng mảnh đúng vị trí","Tệp dữ liệu; offset = chunk_index × chunk_size"),
            ("Database","Lưu thông tin tệp và trạng thái mảnh; truy vấn có tham số","SQLite riêng của peer, chế độ WAL"),
            ("CLI / TUI / Web UI","Tiếp nhận thao tác và hiển thị snapshot tiến độ, peer, log","Cùng sử dụng PeerNode; không giữ logic tải riêng"),
        ],widths=[.20,.43,.37]),
        caption("Bảng 7.2 — Các mô-đun của PeerApp và phạm vi trạng thái chúng sở hữu."),
        peer_flow_diagram(465,125),
        caption("Hình 7.2 — Luồng xử lý một mảnh: khám phá, tải và xác minh, ghi dữ liệu, cập nhật trạng thái, cho phép phục vụ lại."),
        rich("DownloadManager và UploadServer hoạt động đồng thời nên peer có thể tải từ nguồn A trong lúc phục vụ mảnh đã xác minh cho peer B. Sự đồng thời nằm ở các kết nối và worker; một phiên tải có thể lấy nhiều mảnh từ nhiều nguồn. Số nguồn, độ sâu pipeline và số slot upload đều bị giới hạn. Trong hiện thực hiện tại, PeerNode chỉ duy trì một tác vụ download đang hoạt động tại một thời điểm."),
        rich("CommonLib cung cấp Metadata, SHA256, Protocol, Logger và lớp bọc socket cho cả hai chương trình. Đây là thư viện liên kết vào từng tiến trình: việc dùng chung mã không tạo bộ nhớ chung giữa các node. Mutex của StateManager hoặc SQLite chỉ đồng bộ các luồng trong peer sở hữu chúng, không thay thế giao tiếp HTTP/TCP giữa các peer."),
        h2("7.4. Luồng hoạt động từ chia sẻ đến hoàn tất"),
        numbered([
            "Seeder đọc tệp, tính SHA-256 từng mảnh và toàn tệp, rồi sinh metadata JSON. Metadata được chuyển tới bên tải qua kênh riêng; bản thân nó không chứa nội dung tệp.",
            "Seeder đăng ký tệp với UploadServer, đánh dấu các mảnh sẵn có và announce lên tracker bằng File ID, Peer ID, endpoint và bitfield.",
            "Leecher nạp cùng metadata, chuẩn bị tệp đích và trạng thái cục bộ, công bố các mảnh đã có, rồi yêu cầu danh sách nguồn từ tracker.",
            "Leecher mở các kết nối TCP trực tiếp tới nguồn, thực hiện HANDSHAKE/BITFIELD và xin slot phục vụ. Scheduler phân công các mảnh còn thiếu cho những nguồn thích hợp; các mảnh khác nhau có thể được nhận đồng thời.",
            "Mỗi PIECE được kiểm tra index, kích thước và hash trước khi ghi theo offset. Sau khi mảnh hợp lệ được lưu, trạng thái verified được cập nhật; callback announce có throttle giúp peer khác biết thêm nguồn mảnh. UploadServer chỉ phục vụ phần đã có và hợp lệ.",
            "Khi đủ mảnh, peer băm lại toàn tệp để đối chiếu file_hash. Nếu đúng, tệp được công nhận hoàn tất; peer có thể tiếp tục ở lại swarm để seed. Nếu mảnh sai hoặc nguồn lỗi, mảnh chưa hoàn tất được đưa về hàng chờ trong giới hạn retry.",
        ]),
        rich("Ví dụ, L nhận mảnh i từ P1 và mảnh j từ P2 với i ≠ j. Mảnh j có thể tới trước i nhưng vẫn được ghi đúng vị trí nhờ chỉ số tuyệt đối. Sau khi j được xác minh, L có thể phục vụ j cho P3 ngay cả khi i và các mảnh khác còn thiếu. Tracker chỉ tham gia khâu tìm nguồn và cập nhật tình trạng; nó không nằm trên đường truyền nội dung của hai mảnh này."),
        h2("7.5. Quyền sở hữu và lưu trữ trạng thái"),
        table(["Trạng thái","Nơi lưu / chủ sở hữu","Tính chất và phục hồi"],[
            ("Danh mục peer theo tệp","RAM của tracker","Snapshot khám phá có thể trễ; mất khi tracker restart, được tái tạo khi peer announce lại"),
            ("Metadata và hash","Tệp JSON tại các peer","Dùng cùng bố cục mảnh và hash tin cậy làm căn cứ xác minh; không chỉnh sửa cạnh tranh nội dung tệp"),
            ("Bitfield và tiến độ","StateManager + SQLite riêng của từng peer","Mỗi peer công bố phần mình đã xác minh; khi resume cần đọc lại mảnh đã lưu để kiểm tra"),
            ("Nội dung mảnh / tệp đích","Đĩa cục bộ của peer","Ghi theo offset; không mount chung thư mục để thay cho truyền mảnh qua mạng"),
            ("Session và hàng chờ","RAM của DownloadManager/UploadServer","Không phải state bền; được thiết lập lại qua kết nối và tiến độ đã lưu sau restart"),
        ],widths=[.23,.30,.47]),
        caption("Bảng 7.3 — Ranh giới trạng thái phân tán và dữ liệu cục bộ."),
        rich("Bitfield ở tracker là thông tin khám phá do peer công bố, còn bitfield cục bộ gắn với kết quả xác minh của chính peer đó. Thiết kế chấp nhận cập nhật khám phá có độ trễ nhưng không chấp nhận mảnh sai trở thành dữ liệu hoàn tất. Kiểm tra hash bảo đảm toàn vẹn theo metadata; nó không đồng nghĩa với xác thực peer hay đồng thuận phân tán. Mô hình nhất quán và cơ chế khóa được trình bày ở mục 10."),
        h2("7.6. Ánh xạ triển khai"),
        rich("Các tiến trình có thể bố trí trên nhiều máy LAN hoặc trong container. Ví dụ, máy A chứa tracker và peer P1; máy B chứa peer P2, P3. Dù hai peer cùng nằm trên máy B, chúng vẫn có cổng lắng nghe, Peer ID, DB và thư mục dữ liệu độc lập. Endpoint announce phải là IP/hostname mà các peer còn lại truy cập được; địa chỉ 127.0.0.1 không đại diện cho một peer trên máy khác."),
        rich("Cấu hình Docker Compose trong deploy/ tạo một tracker và ba peer với hostname tracker, peer-seeder, peer-leecher-1, peer-leecher-2 trên mạng bridge riêng. Mỗi peer gắn một volume trạng thái riêng. TRACKER_URL trỏ tới tracker trong mạng này; PEER_HOST cung cấp hostname có thể phân giải giữa các container. Tên host trong mạng Compose không tự trở thành địa chỉ truy cập từ LAN bên ngoài."),
        h2("7.7. Đánh đổi kiến trúc và phạm vi chịu lỗi"),
        rich("Tracker tập trung làm cơ chế khám phá đơn giản và dễ quan sát nhưng tạo một điểm phụ thuộc: khi tracker không hoạt động, peer mới khó tìm nguồn và registry cần được xây lại sau restart. Các phiên TCP đã thiết lập có thể tiếp tục nếu nguồn đã biết còn sống và giữ đủ mảnh. Vì vậy tách đường dữ liệu khỏi tracker giúp giảm ảnh hưởng lỗi, nhưng không tạo khả năng sẵn sàng cao cho toàn hệ thống."),
        rich("Khi một nguồn rời mạng, việc chuyển tải chỉ thành công nếu các nguồn còn lại có đủ những mảnh đang thiếu và còn trong ngân sách thử lại. Hệ thống không tự tái tạo một mảnh khi mọi bản sao của mảnh đó đã mất. Kiến trúc hiện tại cũng chưa có tracker sao chép, DHT hoặc NAT traversal tự động; triển khai qua WAN cần endpoint TCP truy cập được. Cơ chế phát hiện lỗi, retry và checkpoint được phân tích tiếp ở mục 11; giới hạn bảo mật ở mục 12."),
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
            "mang lại ba tính chất: định danh theo nội dung không cần máy chủ cấp phát, hai tệp cùng nội dung tự động cùng "
            "định danh (chống trùng lặp), và bất kỳ ai cầm siêu dữ liệu đều xác minh được cả từng mảnh lẫn toàn tệp. "
            "`chunk_hashs` có đúng `num_chunks` phần tử khi sinh từ tệp hợp lệ; mảnh dữ liệu cuối có thể ngắn hơn `chunk_size` nếu kích thước tệp "
            "không chia hết."
        ),
        rich("**Phạm vi tương thích.** Đây là giao thức do nhóm thiết kế lấy ý tưởng từ BitTorrent [3]. Metadata JSON dùng SHA-256 và File ID là hash nội dung; BEP 3 dùng bencoding và info-hash của từ điển info. Vì vậy không thể đưa metadata của đồ án vào qBittorrent thật và kỳ vọng tải được. Với metadata của đồ án, N = ceil(file_size/chunk_size); offset mảnh i = i × chunk_size, độ dài mảnh cuối là phần còn lại."),
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
            "`X-Forwarded-For` → `X-Real-IP` → địa chỉ kết nối khi cần suy ra IP. Tracker thay địa chỉ cục bộ "
            "(`127.0.0.1`, `0.0.0.0`) bằng IP quan sát được khi phía gọi là mạng ngoài. Các header proxy hiện chưa được xác minh nguồn tin cậy; đây không phải cơ chế chống giả mạo."
        ),
        h2("8.3. Giao thức Peer ↔ Peer (TCP thô, khung tin nhị phân)"),
        rich(
            "TCP là một dòng byte liên tục, không phải chuỗi thông điệp: một lần `recv()` có thể trả về nửa khung "
            "hoặc hai khung dính liền. Vì vậy mọi khung đều có tiền tố độ dài và việc đọc dùng `recv_exact` lặp "
            "đến khi đủ số byte, gửi dùng `send_exact`. Mọi số nguyên nhiều byte đều ở **network byte order** "
            "(`htonl`/`ntohl` với uint32; `htons`/`ntohs` với uint16)."
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
                ("0x04", "CANCEL", "`uint32 index`", "Khai báo mã hủy; không phải luồng tải chính"),
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
            "Giới hạn cứng: trường length tối đa **8 MiB** (`MAX_WIRE_MESSAGE_SIZE`, không tính 4 byte prefix), `peer_id` và `file_id` trong handshake tối đa **128 byte**. "
            "Khung vượt giới hạn bị đóng kết nối **trước khi** cấp phát bộ nhớ, nên một peer độc hại không thể ép "
            "đối phương cấp phát 4 GB chỉ bằng một tiền tố độ dài."
        ),
        rich("Với frame có body, length = 1 + payload.size(), không tính prefix 4 byte. REQUEST mang index uint32; PIECE mang index uint32 và dữ liệu, nên chunk_size còn phải phù hợp trần frame. Decoder cũng chấp nhận length = 0 làm keepalive không có byte loại. Bitfield hiện dùng một byte cho một mảnh trên wire, không phải bit-packing như giao thức BitTorrent đầy đủ."),
        h2("8.4. Trình tự một phiên tải"),
        sequence_diagram(430, 215),
        caption("Hình 8.2 — Trình tự bắt tay và trao đổi mảnh giữa leecher, tracker và seeder."),
        rich(
            "Sau khi nhận `BITFIELD`, leecher gửi `INTERESTED`; seeder cấp một slot và trả `UNCHOKE`. Leecher sau đó "
            "gửi tối đa `pipeline_depth` yêu cầu `REQUEST` chưa được trả lời cùng lúc, và bù thêm mỗi khi nhận một "
            "`PIECE`. Nếu seeder hết slot trong `choke_wait_ms`, nó trả `CHOKE` và leecher chuyển sang peer khác — "
            "đây là cơ chế cân bằng tải tự nhiên, không cần bộ điều phối trung tâm."
        ),
    ]


def section_9_discovery():
    return [h1("9. Định danh và khám phá"),
        h2("9.1. Định danh peer và tệp"),
        rich("Peer ID được sinh khi khởi động, có dạng `peer-<port>-<16 ký tự hex>`, kết hợp nguồn ngẫu nhiên, đồng hồ steady và PID. ID giúp phân biệt peer trong registry và tránh kết nối lại chính mình; nó có xác suất trùng rất thấp trong quy mô đồ án nhưng không phải chứng thực mật mã. Peer ID chưa lưu bền nên thay đổi sau restart."),
        rich("File ID là `file_hash`: SHA-256 toàn bộ nội dung, biểu diễn bằng 64 ký tự hex. Để các peer thuộc cùng swarm sử dụng cùng bố cục mảnh, các peer sử dụng **cùng một metadata**. Chỉ có cùng hash tệp nhưng khác chunk_size/danh sách hash chưa đủ để bảo đảm hai peer tương thích giao thức mảnh; bố cục chưa được đưa vào File ID."),
        h2("9.2. Đăng ký và tra cứu"),
        numbered(["Seeder sinh metadata và khởi động UploadServer; announce Peer ID, File ID, IP, port và bitfield.",
                  "Leecher nhận riêng metadata, đăng ký bitfield ban đầu rồi gọi GET /peers theo File ID.",
                  "Danh sách trả về là gợi ý; leecher kiểm tra địa chỉ, bỏ chính mình và kết nối trực tiếp tới peer.",
                  "HANDSHAKE thống nhất File ID; BITFIELD cho biết trạng thái tại phiên; chỉ mảnh có sẵn mới được yêu cầu.",
                  "Sau mỗi mảnh đã xác minh, callback có thể announce với ngưỡng chống gửi dồn 750 ms; heartbeat định kỳ tiếp tục cập nhật trạng thái."]),
        h2("9.3. Liveness và giới hạn của phát hiện lỗi"),
        rich("Peer gửi heartbeat khoảng 15 giây; tracker quét registry mỗi 10 giây và loại bản ghi có tuổi lớn hơn 45 giây theo đồng hồ steady. Khi mạng và lịch CPU bình thường, thời điểm loại thường nằm khoảng 45–55 giây tính từ **announce cuối**, không phải luôn tính từ lúc kill. Nếu kill sau một heartbeat đã lâu thì thời gian chờ tính từ kill ngắn hơn. Không thể dùng riêng timeout để phân biệt chắc chắn peer chết với mạng bị chia cắt."),
        rich("Phía tải phát hiện lỗi qua EOF/lỗi socket hoặc timeout, độc lập với lease trên tracker: không cần chờ tracker loại nguồn mới chuyển tải. Khi nguồn còn đủ dữ liệu, phiên tiếp tục; nếu các mảnh còn thiếu không còn nguồn khả dụng, scheduler có ngân sách khám phá hữu hạn và có thể kết thúc lỗi."),
        h2("9.4. Địa chỉ và chi phí khám phá"),
        rich("Trong LAN phải công bố IP mà máy khác truy cập được bằng `--announce-ip`; 127.0.0.1 chỉ có ý nghĩa trên cùng máy. Với container, `PEER_HOST` là hostname trong mạng Compose. Kết nối WAN cần cấu hình định tuyến/NAT phù hợp; đồ án chưa có NAT traversal tự động."),
        rich("Mỗi announce chứa bitfield O(N) với N mảnh; nhiều peer và nhiều truy vấn danh sách sẽ tăng tải tracker. Vì vậy không suy luận khả năng phục vụ hàng trăm hay hàng nghìn peer từ phép đo 1–3 nguồn. Khi tracker restart, registry trong RAM mất; peer phải announce lại để tái tạo trạng thái."),
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
                ("Mảnh được yêu cầu lại khi đang bay", "Hai luồng cùng ghi một offset", "In-flight tracking; thu hồi và requeue sau lỗi hoặc timeout"),
            ],
            widths=[0.26, 0.32, 0.42],
        ),
        caption("Bảng 10.1 — Các xung đột và cơ chế xử lý."),
        spacer(2),
        h2("10.2. Mô hình nhất quán"),
        rich(
            "Hệ thống áp dụng **nhất quán cuối cùng** cho metadata của swarm: tracker và các peer khác chỉ biết một "
            "peer vừa có thêm mảnh sau announce định kỳ hoặc callback có throttle; lỗi mạng có thể làm thông tin trễ lâu hơn. Với **nội dung**, hệ thống "
            "áp dụng bất biến toàn vẹn: một mảnh chỉ được ghi nhận khi mã băm khớp metadata tin cậy, và toàn tệp chỉ được coi là hoàn "
            "tất khi mã băm tổng khớp. Nói cách khác, trạng thái “ai giữ gì” có thể trễ, nhưng trạng thái “tôi giữ "
            "gì” phải được xác minh lại khi phục hồi. Kiểm tra hash là thuộc tính toàn vẹn, không phải bằng chứng linearizability hay đồng thuận phân tán."
        ),
        rich(
            "Bitfield có **một chủ sở hữu** là peer tương ứng. Tracker cập nhật snapshot theo announce đến sau; chưa có version hoặc phép hợp nhất để gọi đây là CRDT. Announce trễ có thể tạm thay snapshot mới bằng snapshot cũ; heartbeat thành công tiếp theo giúp cập nhật lại. Lamport/vector clock chưa cần cho chỉnh sửa nội dung vì tệp là bất biến, nhưng version/epoch là hướng cải thiện metadata liveness."
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
            "Mỗi phiên giữ tối đa `pipeline_depth` yêu cầu chưa được trả lời (mặc định 3) — nhằm che một phần độ trễ khứ "
            "hồi mà không dồn quá nhiều mảnh vào một kết nối. Số phiên song song bị giới hạn bởi "
            "`max_active_downloads` (mặc định 5), và phía phục vụ giới hạn `max_active_uploads` (mặc định 5) kèm "
            "hàng đợi 64 kết nối. Vượt hàng đợi, kết nối bị đóng ngay thay vì làm chậm cả seeder."
        ),
        rich(
            "**Giới hạn thời gian chờ slot.** Seeder chờ cấp slot tối đa `choke_wait_ms` = 2000 ms cho một client; "
            "nếu không có, nó trả `CHOKE`. Giá trị này phải nhỏ hơn `io_timeout_ms` của phía tải (4000 ms), nếu "
            "không một seeder khỏe nhưng đang bận sẽ bị phía tải coi là đã chết và tiêu tốn ngân sách lỗi — "
            "cần phân biệt thời gian chờ slot với thời gian giữ slot. Cơ chế này chưa bảo đảm công bằng hoặc không đói cho mọi peer."
        ),
        h2("10.5. Trùng lặp, sai thứ tự và tiêu chí trạng thái cuối"),
        rich("TCP giữ thứ tự byte trong một kết nối; nhiều kết nối có thể hoàn tất mảnh theo thứ tự khác nhau. Index mảnh xác định vị trí ghi nên không cần sắp lại toàn bộ luồng tải theo thứ tự đến. Phản hồi phải thuộc tập in-flight của phiên, đúng kích thước và đúng hash; phản hồi ngoài yêu cầu bị xem là lỗi giao thức, không được dùng để tăng tiến độ."),
        rich("Giao thức không định nghĩa ACK riêng cho commit mảnh: PIECE là phản hồi REQUEST, còn verified là trạng thái cục bộ sau kiểm tra và ghi. TCP ACK không chứng minh mảnh đã lưu bền trong DB. Khi mất kết nối, yêu cầu có thể được phát lại; ghi cùng nội dung đúng vào cùng offset có tính lũy đẳng, nhưng không tuyên bố truyền đúng-một-lần trên mạng."),
        rich("Các bất biến cần kiểm tra: một mảnh chỉ do một worker claim trong cùng scheduler; bitfield true chỉ dành cho nội dung đã xác minh; mảnh sai không được tính hoàn thành; số kết nối/slot không vượt giới hạn; tệp cuối phải có SHA-256 đúng metadata. Bằng chứng cuối của phiên gồm hash, tiến độ đủ và dấu vết nguồn đóng góp; không chỉ là thông báo giao diện 100%."),
    ]
