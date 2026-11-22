# Kịch bản demo DS02 — 2 người thao tác trên 2 máy

Đối chiếu `COURSE PROJECT SPECIFICATION.pdf`, §4, §6.2–6.4, §9 và §11. Đây là hướng dẫn diễn tập và thu minh chứng; các bước LAN dưới đây chưa được thực thi trong lượt biên tập tài liệu. Không coi điều kiện PASS dự kiến là kết quả đã chạy.

## 1. Thành viên, phân vai và topology

| Thành viên | MSSV | Vai trò đề xuất |
| --- | --- | --- |
| Hoàng Văn Huynh | B23DCDT131 | Người A: máy A, tracker, nguồn S, metadata, ngắt nguồn và log discovery |
| Nguyễn Gia Bách | B23DCVT042 | Người B: máy B, peer P và L, trạng thái đồng thời, hash, mảnh hỏng và resume |
| Nguyễn Hữu Trường | B23DCVT424 | Giới thiệu triển khai, môi trường, minh chứng đóng góp và trả lời phần hạ tầng; có thể đổi vai thao tác với A/B |

**Hai người thao tác không có nghĩa chỉ hai thành viên tham gia.** §11 yêu cầu cả nhóm tham gia demo/vấn đáp. Trường nên trình bày 45–60 giây về triển khai hoặc giải thích phần việc khi vấn đáp; không cần máy thứ ba.

| Máy | Tiến trình | TCP | Web dashboard | Thư mục trạng thái |
| --- | --- | ---: | ---: | --- |
| A | Tracker T | 18080 HTTP | — | Registry trong RAM |
| A | Peer S | 16881 | 18081 | Riêng cho từng pha |
| B | Peer P | 16882 | 18082 | Riêng cho từng pha |
| B | Peer L | 16883 | 18083 | Riêng cho từng pha |

Có **1 tracker + 3 peer = 4 tiến trình**, đúng yêu cầu riêng DS02. Hai peer trên B vẫn là hai node độc lập với Peer ID, port, DB và output riêng. Không dùng chung database, NFS hoặc thư mục đồng bộ giữa hai máy. Đường điều khiển: peer → tracker qua HTTP. Đường dữ liệu: peer ↔ peer qua TCP, trong đó S trên A thực sự truyền dữ liệu sang B. Tracker không phục vụ nội dung tệp.

Luồng các pha:

1. S có tệp → P tải; L nhận mảnh từ P khi P chưa đủ tệp. Chụp bằng chứng, rồi để P tải đủ để chuẩn bị nguồn thứ hai.
2. Khởi động S và P như hai nguồn đầy đủ → L mới tải song song. Ngắt S giữa chừng; P còn đủ mảnh để L hoàn tất.
3. S cố ý gửi mảnh sai + P tốt → L mới phát hiện hash sai, lấy lại từ P và xác minh toàn tệp.
4. Bổ sung: kill/restart L để resume; quan sát lease của nguồn chết; đầu vào HTTP sai và số liệu hiệu năng.

## 2. Nhịp trình bày

| Thời lượng | Người chính | Việc trình bày |
| --- | --- | --- |
| 3–5 phút | A, B, Trường | Bài toán; 4 tiến trình; control/data plane; phân công và phần việc từng người |
| 0:00–1:30 demo | A + B | Metadata, /peers, P vừa tải vừa upload; để P hoàn tất |
| 1:30–4:00 demo | B + A | Hai nguồn có đóng góp đồng thời, kill S, chuyển nguồn, hash cuối |
| 4:00–5:30 demo | A + B | Mảnh sai bị từ chối, tải lại từ nguồn tốt, hash cuối |
| 5:30–7:30 demo | B | Crash-resume nếu đã diễn tập ổn định |
| 7:30–10:00 demo | A + B | Lease, dữ liệu sai, số liệu đo, kết luận mức đáp ứng |
| Khoảng 5 phút | Cả ba | Vấn đáp và mở commit của từng người |

Thời gian chỉ là dự kiến với tệp 128 MiB và delay dưới đây. Chuyển pha theo điều kiện trạng thái, không theo đồng hồ cố định. Nếu cần rút ngắn, ưu tiên đủ bảy tình huống §6.4 trước; resume và lease là phần bổ sung. Chuẩn bị build, kết nối và cửa sổ trước buổi bảo vệ; không build lại trước mặt hội đồng.

## 3. Preflight trước buổi demo

- Chốt cùng commit trên hai máy: so sánh `git rev-parse HEAD`. Build từ commit đó sau khi các thay đổi code đã ổn định. Không pull, build hoặc chạy test đồng thời với phần demo.
- Khuyến nghị hai máy Linux/Ubuntu hoặc môi trường Linux có TCP truy cập được từ máy kia. Lệnh dưới đây dùng Bash, Python 3, curl, jq, coreutils (`stdbuf`, `sha256sum`), `ss`; scp chỉ cần nếu chọn cách chuyển metadata qua SSH.
- Với WSL2/NAT, kiểm tra cổng vào WSL thực sự truy cập được; nếu chưa cấu hình port forwarding thì dùng Linux trực tiếp hoặc VM bridge. Không chỉ dựa vào ping.
- Hai máy cùng LAN, tắt client isolation/AP isolation nếu Wi-Fi đang chặn máy con. Xác nhận IP thực bằng `ip -br addr`.
- Cho phép TCP 18080/16881 trên A, 16882/16883 trên B trong phạm vi LAN demo; Web dashboard chỉ cần mở tại máy sở hữu. Kiểm tra firewall hiện có, không tắt toàn bộ firewall để chữa lỗi.
- Mỗi máy chạy `ss -ltn` và xác nhận các cổng trên chưa bị dùng. Nếu bận, chọn cổng mới và thay đồng bộ mọi lệnh/URL bên dưới; không kill tiến trình không thuộc demo.
- Trong mỗi pha, chỉ có các peer được nêu hoạt động cho File ID đang thử. Peer từ pha cũ còn chạy có thể làm sai bằng chứng failover hoặc nguồn hỏng.
- Không dùng `--exit-after-download` cho P vì P phải tiếp tục làm nguồn. Đừng mở TUI cùng lúc cần thu stdout của CLI. Dashboard dùng để quan sát, log được ghi riêng.
- Mở sẵn README, báo cáo PDF, CSV benchmark và commit minh chứng của cả ba. Ghi màn hình cả hai máy hoặc chụp snapshot trạng thái kèm log; chưa có ảnh/log thì chưa đánh dấu PASS.

Build trên **mỗi máy**, trước buổi demo nếu chưa có binary Release của commit chốt:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j2
```

### 3.1. Khai báo trong một terminal điều khiển trên mỗi máy

Chạy từ thư mục gốc repo. Thay hai IP ví dụ bằng IP thật. Chọn `RUN_ID` mới cho mỗi lần diễn tập để không dùng nhầm tiến độ cũ. Cùng chuỗi đường dẫn `/tmp/...` trên hai máy vẫn là hai ổ lưu trữ độc lập.

```bash
set -o pipefail
export A_IP=192.168.1.10
export B_IP=192.168.1.20
export RUN_ID=ds02-lan-01
export DEMO_ROOT="/tmp/qbt-$RUN_ID"
export DEMO_TRACKER="http://$A_IP:18080"
export PEER_BIN="$(realpath ./build-release/src/peer/PeerApp)"
export TRACKER_BIN="$(realpath ./build-release/src/tracker/TrackerApp)"
mkdir -p "$DEMO_ROOT"/{incoming,source,logs,pids,evidence}
test -x "$PEER_BIN" && test -x "$TRACKER_BIN"
"$PEER_BIN" --help
git rev-parse HEAD > "$DEMO_ROOT/evidence/commit.txt"
git status --short > "$DEMO_ROOT/evidence/working-tree.txt"
date -Is > "$DEMO_ROOT/evidence/start-time.txt"
```

Hai hàm dưới đây chỉ tồn tại trong terminal, không sửa scripts của dự án. `start_node` lưu PID tiến trình trực tiếp, không chạy qua pipeline `tee`. Dùng tên role mới cho mỗi pha để giữ nguyên log cũ. Khi dừng, hàm kiểm tra executable để tránh kill nhầm PID đã được tái sử dụng.

```bash
start_node() {
  local demo_role="$1"
  shift
  if test -f "$DEMO_ROOT/pids/$demo_role.pid"; then
    echo "Role đã tồn tại; chọn role/RUN_ID mới: $demo_role"
    return 1
  fi
  stdbuf -oL -eL "$@" > "$DEMO_ROOT/logs/$demo_role.log" 2>&1 &
  local demo_pid=$!
  printf '%s\n' "$demo_pid" > "$DEMO_ROOT/pids/$demo_role.pid"
  printf '%s: PID=%s\n' "$demo_role" "$demo_pid"
}

stop_node() {
  local demo_role="$1"
  local demo_signal="${2:-TERM}"
  local demo_pid demo_exe
  demo_pid="$(cat "$DEMO_ROOT/pids/$demo_role.pid")" || return 1
  if ! kill -0 "$demo_pid" 2>/dev/null; then
    echo "$demo_role đã dừng"
    wait "$demo_pid" 2>/dev/null || true
    return 0
  fi
  demo_exe="$(readlink -f "/proc/$demo_pid/exe")"
  if [[ "$demo_exe" != "$PEER_BIN" && "$demo_exe" != "$TRACKER_BIN" ]]; then
    echo "Từ chối kill: PID không còn là binary demo ($demo_pid)"
    return 1
  fi
  date -Is >> "$DEMO_ROOT/evidence/$demo_role-stop.txt"
  kill -s "$demo_signal" "$demo_pid"
  wait "$demo_pid" 2>/dev/null || true
}
```

Giữ terminal điều khiển này để dùng lại biến, hàm và `wait`. Mở terminal khác chỉ để `tail -f` log, hoặc trình duyệt dashboard. Không khởi động hai role trên cùng port đồng thời. `start_node` in PID chưa có nghĩa node đã sẵn sàng: kiểm tra log/health trước bước tiếp theo.

## 4. Pha I — metadata, discovery, upload khi chưa hoàn tất

### 4.1. A chạy tracker và nguồn S

Tệp 128 MiB = 134217728 byte, chunk 256 KiB → 512 mảnh. Dữ liệu ngẫu nhiên giúp tránh nhầm với một tệp rỗng hoặc tệp đã có từ trước.

```bash
head -c 134217728 /dev/urandom > "$DEMO_ROOT/source/demo.bin"
sha256sum "$DEMO_ROOT/source/demo.bin" \
  | tee "$DEMO_ROOT/evidence/source-sha256.txt"

start_node warm-t "$TRACKER_BIN" -p 18080
curl -fsS --max-time 3 "$DEMO_TRACKER/health"

start_node warm-s "$PEER_BIN" --tracker "$DEMO_TRACKER" \
  --announce-ip "$A_IP" -p 16881 \
  --data-dir "$DEMO_ROOT/warm-s/state" \
  --seed "$DEMO_ROOT/source/demo.bin" --chunk-size 262144 \
  --max-uploads 1 --chunk-delay-ms 80 \
  --webui --webui-port 18081
```

Đợi log `Seeding file` rồi đọc metadata. Nếu lệnh chạy trước lúc file được sinh, chờ log và thực hiện lại, không dùng metadata cũ.

```bash
jq '{file_name,file_size,chunk_size,num_chunks,file_hash,tracker_url,
     chunk_hash_count:(.chunk_hashs|length)}' \
  "$DEMO_ROOT/source/demo.bin.torrent.json" \
  | tee "$DEMO_ROOT/evidence/metadata-summary.json"
export FILE_ID="$(jq -r .file_hash "$DEMO_ROOT/source/demo.bin.torrent.json")"
curl -fsS --max-time 3 "$DEMO_TRACKER/peers?file_id=$FILE_ID" \
  | tee "$DEMO_ROOT/evidence/warm-discovery.json" | jq .
```

**A nói:** “Tracker chỉ trả IP, port và bitfield. Metadata có hash từng mảnh và hash tổng; nội dung 128 MiB đi trực tiếp qua cổng TCP của peer.”

Chuyển **chỉ JSON metadata** sang B. Ví dụ nếu B có SSH, thay `your-user` bằng tài khoản trên B:

```bash
scp "$DEMO_ROOT/source/demo.bin.torrent.json" \
  "your-user@$B_IP:$DEMO_ROOT/incoming/demo.bin.torrent.json"
```

Nếu không có SSH, A xuất metadata bằng nút `.torrent` trên dashboard rồi chuyển file JSON bằng USB/kênh khác. Không copy `demo.bin`, DB hoặc thư mục output sang B: P phải nhận nội dung qua P2P trong pha này.

### 4.2. B kiểm tra tracker và chạy P

```bash
curl -fsS --max-time 3 "$DEMO_TRACKER/health"
export FILE_ID="$(jq -r .file_hash "$DEMO_ROOT/incoming/demo.bin.torrent.json")"
test "$(jq -r .tracker_url "$DEMO_ROOT/incoming/demo.bin.torrent.json")" = "$DEMO_TRACKER"

start_node warm-p "$PEER_BIN" --tracker "$DEMO_TRACKER" \
  --announce-ip "$B_IP" -p 16882 \
  --data-dir "$DEMO_ROOT/warm-p/state" \
  --download "$DEMO_ROOT/incoming/demo.bin.torrent.json" \
  --output-dir "$DEMO_ROOT/warm-p/out" \
  --max-downloads 1 --max-uploads 2 --pipeline-depth 1 \
  --chunk-delay-ms 150 --io-timeout-ms 6000 \
  --webui --webui-port 18082
```

Mở `http://localhost:18082` trên B. Chờ P có khoảng 20–100/512 mảnh, vẫn chưa đủ tệp, rồi chạy L. S chỉ có một upload slot đang phục vụ P; L có thể nhận CHOKE từ S và dùng mảnh P đã xác minh.

```bash
curl -fsS --max-time 3 http://localhost:18082/api/status | jq \
  '{peer_id,uploaded_bytes,torrents,download_peers}'

start_node warm-l "$PEER_BIN" --tracker "$DEMO_TRACKER" \
  --announce-ip "$B_IP" -p 16883 \
  --data-dir "$DEMO_ROOT/warm-l/state" \
  --download "$DEMO_ROOT/incoming/demo.bin.torrent.json" \
  --output-dir "$DEMO_ROOT/warm-l/out" \
  --max-downloads 2 --pipeline-depth 1 --io-timeout-ms 6000 \
  --webui --webui-port 18083
```

Khi P đã phục vụ mảnh, lưu **một snapshot duy nhất chứa cả tiến độ và lượng upload**:

```bash
curl -fsS --max-time 3 http://localhost:18082/api/status \
  > "$DEMO_ROOT/evidence/partial-p.json"
jq '{peer_id,uploaded_bytes,torrents,upload_peers}' \
  "$DEMO_ROOT/evidence/partial-p.json"
jq -e --arg fid "$FILE_ID" \
  '(.uploaded_bytes > 0) and
   any(.torrents[]; .file_id == $fid and
       .completed_chunks > 0 and .completed_chunks < .num_chunks)' \
  "$DEMO_ROOT/evidence/partial-p.json"

curl -fsS --max-time 3 http://localhost:18083/api/status \
  > "$DEMO_ROOT/evidence/partial-l.json"
jq '.download_peers | map(select(.chunks_downloaded > 0))' \
  "$DEMO_ROOT/evidence/partial-l.json"
```

**PASS:** biểu thức trả `true`; L có nguồn P đúng Peer ID/port và đã nhận mảnh. Nếu quá sớm, đợi tiến độ rồi chụp lại. Nếu P đã 100% trước snapshot, pha này **chưa đạt**: diễn tập lại với RUN_ID mới/tệp lớn hơn hoặc delay S lớn hơn. Không dùng ảnh sau 100% thay cho bằng chứng upload khi chưa hoàn tất.

**B nói:** “P mới có một phần tệp nhưng uploaded_bytes đã tăng và L đang nhận mảnh từ P. UploadServer chỉ phục vụ mảnh đã xác minh, không đợi toàn bộ tệp.”

### 4.3. Chuẩn bị hai nguồn đầy đủ

B dừng L warm-up sau khi có bằng chứng; để P tiếp tục tải đủ. Đây là chuyển pha, không đánh dấu tệp của L warm-up là hoàn tất.

```bash
stop_node warm-l
curl -fsS --max-time 3 http://localhost:18082/api/status | jq \
  '.torrents[] | {completed_chunks,num_chunks,mode}'
```

Khi P có 512/512, log `Download complete and verified` và `Successfully downloaded file`, kiểm tra hash độc lập:

```bash
printf '%s  %s\n' "$FILE_ID" "$DEMO_ROOT/warm-p/out/demo.bin" \
  | sha256sum -c -
cp "$DEMO_ROOT/incoming/demo.bin.torrent.json" \
  "$DEMO_ROOT/warm-p/out/demo.bin.torrent.json"
stop_node warm-p
```

A dừng S warm-up và tracker **sau khi B xác nhận P đã đủ tệp**:

```bash
stop_node warm-s
stop_node warm-t
```

Tracker restart ở ranh giới pha để bỏ registry cũ; đây là thao tác chuẩn bị, không trình bày như failover tự động của tracker.

## 5. Pha II — hai nguồn song song, lỗi live, chuyển nguồn và hash

### 5.1. A khởi động T và S; B khởi động P đầy đủ

Trên A:

```bash
start_node main-t "$TRACKER_BIN" -p 18080
curl -fsS --max-time 3 "$DEMO_TRACKER/health"
start_node main-s "$PEER_BIN" --tracker "$DEMO_TRACKER" \
  --announce-ip "$A_IP" -p 16881 \
  --data-dir "$DEMO_ROOT/main-s/state" \
  --seed "$DEMO_ROOT/source/demo.bin" --chunk-size 262144 \
  --max-uploads 4 --chunk-delay-ms 200 \
  --webui --webui-port 18081
```

Trên B, P seed tệp vừa tải qua mạng ở pha I, cùng metadata và bố cục mảnh:

```bash
start_node main-p "$PEER_BIN" --tracker "$DEMO_TRACKER" \
  --announce-ip "$B_IP" -p 16882 \
  --data-dir "$DEMO_ROOT/main-p/state" \
  --seed "$DEMO_ROOT/warm-p/out/demo.bin" --chunk-size 262144 \
  --max-uploads 4 --chunk-delay-ms 200 \
  --webui --webui-port 18082
curl -fsS --max-time 3 "$DEMO_TRACKER/peers?file_id=$FILE_ID" \
  | tee "$DEMO_ROOT/evidence/main-sources.json" \
  | jq 'map({peer_id,ip,port,have:([.bitfield[]|select(.)]|length)})'
```

Phải thấy hai Peer ID khác nhau, địa chỉ A:16881 và B:16882, mỗi nguồn có đủ 512 mảnh trước khi chạy L. Nếu chưa đủ, chờ announce/log; không tiếp tục khi chỉ thấy một nguồn.

### 5.2. B chạy L mới và chứng minh có hoạt động song song

```bash
start_node main-l "$PEER_BIN" --tracker "$DEMO_TRACKER" \
  --announce-ip "$B_IP" -p 16883 \
  --data-dir "$DEMO_ROOT/main-l/state" \
  --download "$DEMO_ROOT/incoming/demo.bin.torrent.json" \
  --output-dir "$DEMO_ROOT/main-l/out" \
  --max-downloads 2 --pipeline-depth 1 --io-timeout-ms 6000 \
  --webui --webui-port 18083
```

Khi có tiến độ, lưu hai snapshot cách nhau khoảng 2 giây; thực hiện lệnh lần hai sau khi quan sát tiến độ tăng:

```bash
curl -fsS --max-time 3 http://localhost:18083/api/status \
  > "$DEMO_ROOT/evidence/parallel-1.json"
# Quan sát khoảng 2 giây rồi chạy lệnh tiếp theo.
curl -fsS --max-time 3 http://localhost:18083/api/status \
  > "$DEMO_ROOT/evidence/parallel-2.json"
jq '{torrents,download_peers}' "$DEMO_ROOT/evidence/parallel-2.json"

python3 - "$DEMO_ROOT/evidence/parallel-1.json" \
  "$DEMO_ROOT/evidence/parallel-2.json" <<'PY'
import json, sys
first, second = (json.load(open(p)) for p in sys.argv[1:])
old = {p['peer_id']: p['chunks_downloaded'] for p in first['download_peers']}
growing = [p for p in second['download_peers']
           if p['connected'] and p['chunks_downloaded'] > old.get(p['peer_id'], 0)]
print([(p['peer_id'], p['ip'], p['port'], p['chunks_downloaded']) for p in growing])
assert len(growing) >= 2, 'Chưa có bằng chứng hai nguồn cùng tiến triển trong cửa sổ đo'
PY
```

**PASS:** hai nguồn khác nhau đều tăng số mảnh trong cùng cửa sổ quan sát, có kết nối hoạt động; L vẫn chưa đủ tệp. Chỉ có hai địa chỉ trong `/peers` hoặc hai nguồn đóng góp lần lượt chưa đủ chứng minh phần song song.

**B nói:** “Đây là hai nguồn đã thực sự trả mảnh, không chỉ xuất hiện trong danh sách tracker. Tập claimed/in-flight tránh hai worker cùng nhận một mảnh; mảnh được ghi theo offset và kiểm tra hash.”

### 5.3. A kill nguồn S trong khi L đang tải

B xác nhận tiến độ khoảng 10–40% và P có đủ tệp; A mới thực hiện. Lưu identity của S trước khi ngắt:

```bash
curl -fsS --max-time 3 http://localhost:18081/api/status \
  > "$DEMO_ROOT/evidence/main-s-before-kill.json"
export DEAD_PEER_ID="$(jq -r .peer_id "$DEMO_ROOT/evidence/main-s-before-kill.json")"
date -Is | tee "$DEMO_ROOT/evidence/source-crash-time.txt"
stop_node main-s KILL
```

B quan sát log L và chụp tiến độ tiếp tục tăng sau lỗi kết nối:

```bash
tail -n 25 "$DEMO_ROOT/logs/main-l.log"
curl -fsS --max-time 3 http://localhost:18083/api/status \
  | tee "$DEMO_ROOT/evidence/after-source-crash.json" \
  | jq '{torrents,download_peers}'
```

**PASS:** ngắt đúng PID S khi L <100%; L phát hiện EOF/lỗi/timeout, các mảnh còn thiếu tiếp tục nhận từ P mà không khởi động lại L. Đối chiếu bộ đếm nguồn P trước/sau kill. Không gọi thao tác restart thủ công là failover tự động.

**A nói:** “Timeout phía tải và lease tracker là hai cơ chế khác nhau. L không cần chờ 45 giây để tracker xóa S; nó có thể chuyển phần việc sang P sau lỗi phiên TCP.”

### 5.4. B kiểm tra đầu ra hoàn chỉnh

Đợi L đủ 512/512 và log báo hoàn tất có xác minh, sau đó:

```bash
printf '%s  %s\n' "$FILE_ID" "$DEMO_ROOT/main-l/out/demo.bin" \
  | sha256sum -c - | tee "$DEMO_ROOT/evidence/main-final-hash.txt"
rg 'Contributed|Multi-source summary|Download complete and verified' \
  "$DEMO_ROOT/logs/main-l.log" \
  | tee "$DEMO_ROOT/evidence/main-contributors.txt"
```

Nếu máy chưa có `rg`, dùng `grep -E` với cùng biểu thức. **PASS:** hash đầu ra khớp `file_hash`; hai snapshot trước kill đã chứng minh hai nguồn cùng góp mảnh; snapshot sau kill chứng minh tiến độ tiếp tục từ P. Log tổng kết là bằng chứng bổ sung: bộ đếm theo session có thể bị thay khi nguồn lỗi được reconnect, nên không bắt buộc summary cuối vẫn giữ đủ lịch sử đóng góp của S đã chết. Ghép ở đây là ghi mảnh trực tiếp đúng offset, không cần tạo 512 tệp tạm rồi nối lại.

### 5.5. A quan sát lease trong lúc B chờ hoàn tất

Chạy lại lệnh dưới đây sau từng khoảng quan sát, không cần giữ demo đứng chờ một vòng sleep dài:

```bash
date -Is
curl -fsS --max-time 3 "$DEMO_TRACKER/peers?file_id=$FILE_ID" \
  | tee "$DEMO_ROOT/evidence/lease-peers.json" \
  | jq --arg dead "$DEAD_PEER_ID" 'map(select(.peer_id == $dead))'
```

Ban đầu có thể vẫn thấy S; sau quá hạn trả `[]`. Ngưỡng 45 giây tính từ announce cuối, quét khoảng 10 giây; không yêu cầu luôn đúng 50 giây tính từ lúc kill. Dùng timestamp announce cuối trong log tracker để giải thích. Chụp trước và sau bằng tên file khác nếu cần giữ cả hai bằng chứng. Nếu tracker bị restart trước khi quan sát thì việc S biến mất **không còn là bằng chứng lease**.

## 6. Pha III — mảnh hỏng bị loại, tải lại từ nguồn tốt

Sau khi đã lưu hash và lease ở pha II, B dừng `main-l`, `main-p`; A dừng `main-t`. S đã bị kill. Kiểm tra không còn peer pha II phục vụ File ID này.

```bash
# B
stop_node main-l
stop_node main-p
```

```bash
# A
stop_node main-t
start_node corrupt-t "$TRACKER_BIN" -p 18080
curl -fsS --max-time 3 "$DEMO_TRACKER/health"
export BAD_CHUNKS="$(python3 -c \
  'import json,sys; n=json.load(open(sys.argv[1]))["num_chunks"]; print(",".join(map(str,range(n))))' \
  "$DEMO_ROOT/source/demo.bin.torrent.json")"
start_node corrupt-s "$PEER_BIN" --tracker "$DEMO_TRACKER" \
  --announce-ip "$A_IP" -p 16881 \
  --data-dir "$DEMO_ROOT/corrupt-s/state" \
  --seed "$DEMO_ROOT/source/demo.bin" --chunk-size 262144 \
  --corrupt-chunks "$BAD_CHUNKS" --chunk-delay-ms 20 --max-uploads 4 \
  --webui --webui-port 18081
```

Chỉ làm sai payload upload bằng cờ fault injection, **không sửa tệp nguồn hoặc metadata**. Cố ý hỏng mọi chỉ số giúp lần chọn mảnh ngẫu nhiên nào từ S cũng có cơ hội tạo mismatch, thay vì hy vọng scheduler chọn đúng mảnh 0.

Trên B:

```bash
start_node corrupt-p "$PEER_BIN" --tracker "$DEMO_TRACKER" \
  --announce-ip "$B_IP" -p 16882 \
  --data-dir "$DEMO_ROOT/corrupt-p/state" \
  --seed "$DEMO_ROOT/warm-p/out/demo.bin" --chunk-size 262144 \
  --chunk-delay-ms 80 --max-uploads 4 \
  --webui --webui-port 18082
curl -fsS --max-time 3 "$DEMO_TRACKER/peers?file_id=$FILE_ID" | jq .
# Chỉ chạy L khi cả hai nguồn đã announce và log seed thành công.
start_node corrupt-l "$PEER_BIN" --tracker "$DEMO_TRACKER" \
  --announce-ip "$B_IP" -p 16883 \
  --data-dir "$DEMO_ROOT/corrupt-l/state" \
  --download "$DEMO_ROOT/incoming/demo.bin.torrent.json" \
  --output-dir "$DEMO_ROOT/corrupt-l/out" \
  --max-downloads 2 --pipeline-depth 1 --io-timeout-ms 6000 \
  --webui --webui-port 18083
```

Trong lúc tải, mở log:

```bash
rg 'SHA-256 mismatch|blacklisted|Contributed|Download complete' \
  "$DEMO_ROOT/logs/corrupt-l.log" \
  | tee "$DEMO_ROOT/evidence/corruption-events.txt"
```

Đợi hoàn tất rồi kiểm tra:

```bash
printf '%s  %s\n' "$FILE_ID" "$DEMO_ROOT/corrupt-l/out/demo.bin" \
  | sha256sum -c - | tee "$DEMO_ROOT/evidence/corrupt-final-hash.txt"
```

**PASS:** ít nhất một mismatch có chunk index và nguồn; mảnh bị đưa lại hàng chờ/blacklist theo cặp nguồn–mảnh; nguồn P tốt góp mảnh; file cuối đúng hash. Nếu không có mismatch, chỉ hash đúng chưa đủ: kiểm tra L thực sự kết nối S hỏng và chạy lại pha với role/thư mục mới. Nếu chỉ có nguồn hỏng, kết quả đúng phải là lỗi tải, không phải “cố ghép” thành công.

**B nói:** “Hash được so với metadata trước khi ghi nhận mảnh. Blacklist này áp dụng cho một mảnh tại một peer; hash không chứng minh danh tính peer hoặc bảo vệ khỏi việc metadata cũng bị thay.”

## 7. Bổ sung — crash-resume của L

Pha này dùng **chỉ một nguồn tốt P** để dễ kiểm tra số mảnh tải lại. Nó là lỗi thứ ba ngoài mất nguồn và mảnh sai. Test crash-resume tự động bị SKIP trên Linux trong log cũ nên cần bằng chứng live nếu muốn khẳng định tính năng hoạt động ở môi trường này.

Chuyển pha: B dừng `corrupt-l`, `corrupt-p`; A dừng `corrupt-s`, `corrupt-t`, rồi mở tracker mới. Không để nguồn hỏng lẫn vào phiên resume.

```bash
# B
stop_node corrupt-l
stop_node corrupt-p
```

```bash
# A
stop_node corrupt-s
stop_node corrupt-t
start_node resume-t "$TRACKER_BIN" -p 18080
curl -fsS --max-time 3 "$DEMO_TRACKER/health"
```

Trên B:

```bash
start_node resume-p "$PEER_BIN" --tracker "$DEMO_TRACKER" \
  --announce-ip "$B_IP" -p 16882 \
  --data-dir "$DEMO_ROOT/resume-p/state" \
  --seed "$DEMO_ROOT/warm-p/out/demo.bin" --chunk-size 262144 \
  --chunk-delay-ms 80 --max-uploads 4 \
  --webui --webui-port 18082
# Chờ P xuất hiện trong /peers rồi chạy L.
curl -fsS --max-time 3 "$DEMO_TRACKER/peers?file_id=$FILE_ID" | jq .
start_node resume-l1 "$PEER_BIN" --tracker "$DEMO_TRACKER" \
  --announce-ip "$B_IP" -p 16883 \
  --data-dir "$DEMO_ROOT/resume-l/state" \
  --download "$DEMO_ROOT/incoming/demo.bin.torrent.json" \
  --output-dir "$DEMO_ROOT/resume-l/out" \
  --max-downloads 1 --pipeline-depth 1 --io-timeout-ms 6000 \
  --webui --webui-port 18083
```

Khi L khoảng 10–30%, lưu tiến độ rồi kill đúng L:

```bash
curl -fsS --max-time 3 http://localhost:18083/api/status \
  > "$DEMO_ROOT/evidence/resume-before-kill.json"
stop_node resume-l1 KILL

python3 - "$DEMO_ROOT/resume-l/state/peer.db" "$FILE_ID" \
  "$DEMO_ROOT/evidence/resume-persisted.json" \
  "$DEMO_ROOT/incoming/demo.bin.torrent.json" <<'PY'
import json, sqlite3, sys
db = sqlite3.connect('file:' + sys.argv[1] + '?mode=ro', uri=True)
indices = [r[0] for r in db.execute(
    "SELECT chunk_index FROM chunks WHERE file_id=? AND status='verified' ORDER BY chunk_index",
    (sys.argv[2],))]
json.dump(indices, open(sys.argv[3], 'w'))
print('Verified đã lưu sau kill:', len(indices))
n = json.load(open(sys.argv[4]))['num_chunks']
assert 0 < len(indices) < n, 'Cần kill trong khi còn đang tải'
PY
```

Chạy lại với **cùng metadata, DB/data-dir và output-dir**, chỉ đổi tên role/log:

```bash
start_node resume-l2 "$PEER_BIN" --tracker "$DEMO_TRACKER" \
  --announce-ip "$B_IP" -p 16883 \
  --data-dir "$DEMO_ROOT/resume-l/state" \
  --download "$DEMO_ROOT/incoming/demo.bin.torrent.json" \
  --output-dir "$DEMO_ROOT/resume-l/out" \
  --max-downloads 1 --pipeline-depth 1 --io-timeout-ms 6000 \
  --webui --webui-port 18083
```

Đợi hoàn tất, xác minh hash và đối chiếu tập mảnh tải mới:

```bash
printf '%s  %s\n' "$FILE_ID" "$DEMO_ROOT/resume-l/out/demo.bin" \
  | sha256sum -c - | tee "$DEMO_ROOT/evidence/resume-final-hash.txt"
python3 - "$DEMO_ROOT/evidence/resume-persisted.json" \
  "$DEMO_ROOT/logs/resume-l2.log" \
  "$DEMO_ROOT/incoming/demo.bin.torrent.json" <<'PY'
import json, re, sys
saved = set(json.load(open(sys.argv[1])))
text = open(sys.argv[2]).read()
new = set(map(int, re.findall(r'Verified chunk (\d+) for file', text)))
n = json.load(open(sys.argv[3]))['num_chunks']
print('Đã lưu:', len(saved), 'Tải mới:', len(new), 'Tổng:', n)
assert not saved.intersection(new), 'Mảnh verified còn nguyên đã bị tải lại'
assert saved.union(new) == set(range(n)), 'Chưa có bằng chứng đủ mọi mảnh'
assert len(new) == n-len(saved)
PY
```

**PASS:** dữ liệu đã lưu được giữ lại, chỉ tải phần thiếu, hash tổng đúng. Peer ID thay sau restart không làm mất tiến độ vì state gắn File ID/data-dir. Không lấy dòng “Starting multi-peer download (...512 chunks)” làm bằng chứng đã tải lại đủ 512 mảnh: đó là tổng số mảnh của metadata. Nếu sửa tệp trên đĩa giữa hai lần chạy thì một số mảnh đã lưu có thể phải tải lại hợp lệ; pha này giữ nguyên tệp để kiểm tra resume đơn thuần.

## 8. Bổ sung — đầu vào sai và hiệu năng

### 8.1. Một yêu cầu sai tới tracker

A hoặc B thực hiện khi tracker đang chạy, lưu cả status code và body:

```bash
curl -sS --max-time 3 -o "$DEMO_ROOT/evidence/invalid-announce-body.json" \
  -w 'HTTP %{http_code}\n' \
  -H 'Content-Type: application/json' \
  -d '{"peer_id":"invalid-demo","file_id":"demo","port":70000,"bitfield":[]}' \
  "$DEMO_TRACKER/announce" \
  | tee "$DEMO_ROOT/evidence/invalid-announce-status.txt"
cat "$DEMO_ROOT/evidence/invalid-announce-body.json"
curl -fsS --max-time 3 "$DEMO_TRACKER/health"
```

**PASS:** HTTP 400 và tracker vẫn khỏe. Đây là kiểm tra dữ liệu sai; không gọi nó là xác thực người dùng. Bản nguồn được đối chiếu còn thiếu xác thực/ACL đầy đủ và không có TLS. Các guard đường dẫn đã có kết quả kiểm tra bổ sung trong `docs/evidence/web-smoke.json`; kết quả đó chưa chứng minh mọi trường hợp đầu vào đều an toàn. Khi đổi code, cần chạy lại kiểm tra trên commit cuối.

### 8.2. Trình bày kết quả hiệu năng đã lưu

Mở `docs/bench/results.csv` và mục 15 báo cáo. CSV có 12 cấu hình; tệp 104857600 byte là 100 MiB. Nêu rõ: loopback, delay ứng dụng 50 ms/mảnh, một mẫu/cấu hình; số liệu này không phải kết quả đo live trên hai máy hôm nay.

Nếu có thời gian diễn tập riêng sau khi chốt code:

```bash
QBT_BUILD_DIR="$PWD/build-release" \
  BENCH_CSV="$DEMO_ROOT/evidence/bench-repeat.csv" \
  ./scripts/bench.sh > "$DEMO_ROOT/evidence/bench-repeat.log" 2>&1
```

Không chạy benchmark này cùng lúc phần demo live vì nó tự tạo thêm swarm và làm thay đổi tài nguyên đo. Nó không thay thế bằng chứng mạng hai máy.

## 9. Ma trận PASS/FAIL theo đề bài

| §6.4 yêu cầu bắt buộc | Bằng chứng | PASS khi |
| --- | --- | --- |
| Tạo metadata và chia sẻ | metadata-summary.json, /peers, log S | Đủ trường, số hash bằng num_chunks; File ID đúng; seeder đã đăng ký |
| Tải song song từ ≥2 peer | parallel-1/2.json, log Contributed | Hai Peer ID cùng tăng bộ đếm trong cùng cửa sổ; đúng output |
| Upload khi chưa tải đủ | partial-p.json + partial-l.json | P <100% nhưng uploaded_bytes >0; L nhận mảnh từ P |
| Dừng nguồn khi đang tải | source-crash-time.txt, stop PID, snapshot L | Kill S trong lúc L chưa đủ tệp |
| Chuyển nguồn | after-source-crash.json và log L | L không restart; P tiếp tục góp mảnh; tiến độ tăng |
| Phát hiện mảnh sai | corruption-events.txt | Có mismatch và requeue/blacklist; lấy lại từ P tốt |
| Ghép và hash tổng | main/corrupt-final-hash.txt | SHA-256 đầu ra khớp metadata; không chỉ đúng số byte |

Yêu cầu chung cũng phải nhìn thấy: ≥3 peer độc lập, mạng thật giữa A và B, một pha đồng thời, ít nhất hai tình huống lỗi của thiết kế (mất nguồn và mảnh sai), một lỗi live, log và phần việc của cả ba. Resume và lease là minh chứng thêm.

## 10. Xử lý sự cố trong buổi demo

| Hiện tượng | Kiểm tra và cách xử lý |
| --- | --- |
| B không gọi được /health | IP tracker, cùng LAN, firewall, tracker PID/log và port 18080; ping thành công chưa đủ |
| /peers có IP 127.0.0.1 hoặc IP sai | Sửa --announce-ip; restart đúng peer; kiểm tra từ máy còn lại. Không đổi về localhost cho toàn demo |
| /health được nhưng không nhận mảnh từ S | Kiểm tra TCP 16881 và địa chỉ S; đọc lỗi handshake/timeout; không nhầm cổng HTTP với cổng upload |
| Cổng bận | Xác định tiến trình qua ss; chọn cổng khác hoặc dừng đúng role demo của mình, không pkill/killall |
| Tải xong quá nhanh | Tăng chunk-delay-ms hoặc file size trong lần diễn tập mới; giữ io-timeout lớn hơn thời gian phục vụ một mảnh |
| L chỉ nhận từ một nguồn | Chờ cả hai seeder announce trước khi chạy L; fresh data-dir/output; max-downloads=2; hai nguồn đủ mảnh và có slot |
| Không chứng minh được P upload trước 100% | Chạy L sớm hơn trong warm-up; giữ S max-uploads=1; đừng để P hoàn tất trước khi chụp |
| Kill S rồi L thiếu nguồn | Kiểm tra P đã đủ 512/512 trước pha II. Nếu chưa đủ thì đây là mất bản sao duy nhất của vài mảnh, không thể yêu cầu scheduler tạo dữ liệu |
| Không thấy mismatch | Kiểm tra --corrupt-chunks, đúng binary và đúng peer; S hỏng phải kết nối L; dùng L mới và role mới khi diễn tập lại |
| Resume quay về đầu | Cùng data-dir/DB, output-dir, metadata; không xóa output hoặc chạy lại bằng thư mục khác; kiểm tra mảnh nào bị invalidate |
| Peer chết còn trong /peers | Đợi TTL tính từ announce cuối và chu kỳ quét; không restart tracker để giả lập cleanup |
| Log không đúng nội dung kỳ vọng | Ghi nhận commit/binary, đọc hành vi thật; không đánh dấu PASS theo kịch bản nếu thiếu bằng chứng |

Nếu phải diễn tập lại một pha, chọn role/thư mục mới và dừng hết peer pha cũ. Các lệnh ở trên cố ý không xóa dữ liệu tự động để giữ bằng chứng và tránh tác động công việc khác.

## 11. Thu bằng chứng và kết thúc

Trên B, sau các pha đang dùng:

```bash
stop_node resume-l2
stop_node resume-p
```

Trên A:

```bash
stop_node resume-t
```

Nếu đã bỏ qua pha resume thì dừng đúng các role còn chạy ở pha cuối; không gọi tên role chưa tạo. Hai máy lưu riêng log và evidence, kèm file JSON metadata, commit, trạng thái working tree, IP/port và thời điểm chạy:

```bash
tar -czf "$PWD/demo-$RUN_ID-$(hostname).tar.gz" \
  -C "$DEMO_ROOT" evidence logs incoming
```

Không đưa file nguồn 128 MiB/DB hoặc gói log không kiểm tra vào Git. Chọn các bằng chứng cần thiết sau khi rà, đặt tên theo máy/pha nếu lưu vào `docs/evidence/`. Mỗi dòng PASS phải truy được về snapshot hoặc log thực. Giữ `/tmp/qbt-...` tới lúc kiểm tra xong, rồi tự dọn đúng thư mục của lần demo.

## 12. Câu hỏi vấn đáp và ý trả lời

| Câu hỏi | Ý trả lời cần nắm |
| --- | --- |
| Hai máy sao có ba peer? | B có hai PeerApp độc lập, port/Peer ID/DB riêng; toàn hệ có bốn tiến trình. CommonLib và tab trình duyệt không tính là peer |
| Tracker có chuyển nội dung không? | Không; HTTP đăng ký/khám phá, nội dung mảnh qua socket TCP của peer. Mở /peers và log đóng góp/endpoint để chứng minh |
| Tại sao L biết P có mảnh mới? | Callback announce có throttle và heartbeat; tracker bitfield có thể trễ, session/discovery cập nhật thông tin; không broadcast mọi mảnh tới mọi peer |
| Đây có phải CRDT/strong consistency? | Không có phép merge/version để gọi bitfield là CRDT. Snapshot một chủ sở hữu có thể trễ; SHA-256 là toàn vẹn nội dung, không phải đồng thuận hay linearizability |
| Hai worker có tải cùng mảnh không? | Claim từ pending sang claimed/in-flight dưới mutex; kiểm tra phản hồi index và chỉ requeue khi cần; mảnh ghi theo offset |
| Mất nguồn có luôn hoàn tất không? | Chỉ khi các nguồn còn lại giữ đủ mọi mảnh và khả dụng trong ngân sách retry/discovery. P phải đầy đủ trước khi kill S trong pha II |
| Vì sao không chờ tracker xóa S? | Lỗi TCP/timeout phía download phát hiện nhanh hơn lease; registry còn stale không ngăn chuyển sang nguồn đã biết |
| TCP có còn cần retry không? | TCP xử lý truyền byte trong kết nối; ứng dụng vẫn cần retry cho crash, timeout, mảnh sai và chuyển sang peer khác |
| Mảnh sai đến có được ghi không? | Kiểm tra size/index/hash trước khi ghi nhận; blacklist cặp mảnh–nguồn và requeue; toàn tệp còn được hash lần cuối |
| SHA-256 có xác thực nguồn không? | Không. Metadata phải tin cậy; nếu cả metadata bị thay thì hash không bảo vệ danh tính. HANDSHAKE hiện cũng chưa chứng minh sở hữu Peer ID |
| Resume cần giữ gì? | Metadata, data-dir/DB và tệp đích. Đọc lại/băm lại mảnh đã lưu rồi chỉ tải phần thiếu hoặc hỏng; kill process không tương đương mất điện |
| Tracker chết thì sao? | Kết nối peer đã biết có thể tiếp tục nếu còn nguồn; peer mới không discovery được. Registry trong RAM tái tạo sau re-announce; chưa có replicated tracker |
| Có dùng trực tiếp với qBittorrent không? | Không; JSON + SHA-256 + mã khung riêng là mô phỏng ý tưởng, không tương thích đầy đủ BEP 3/bencoding/info-hash |
| Rarest-first/tit-for-tat đã có chưa? | Có random-first rồi rarest-first theo thông tin session đã biết; CHOKE/UNCHOKE giới hạn slot chưa đồng nghĩa tit-for-tat theo đóng góp |
| Số liệu 1,95×/2,91× chứng minh gì? | Lợi ích thêm nguồn trong một phép đo loopback có delay kiểm soát; chưa chứng minh WAN, công bằng hay scaling lớn. Dùng MiB và nêu chỉ một mẫu/cấu hình |
| Bảo mật đáp ứng tới đâu? | Có kiểm tra frame/size/index/hash và prepared statement; còn thiếu auth/ACL đầy đủ, TLS và hardening HTTP. Nêu đúng trạng thái commit cuối |
| Mỗi người làm gì? | Tự mở commit/PR phần việc ở mục 2 báo cáo; giải thích một đường đi cụ thể, không chỉ đọc bảng phân công |
