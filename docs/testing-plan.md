# Kế hoạch và thiết kế kiểm thử (Unit Test & Load Test)

Yêu cầu bắt buộc:
- Đầy đủ Unit Test với độ bao phủ mã nguồn (**Code Coverage > 90%**).
- Có bài kiểm thử khả năng chịu tải của hệ thống (**Load / Stress Test**).
- Đảm bảo quy trình phát triển và commit hàng tuần.

Tài liệu tham chiếu trực tiếp tới các yêu cầu `FR-01..FR-11`, các đề xuất thiết kế `P-01..P-11` trong [requirements.md](requirements.md), kiến trúc module trong [technical-design.md](technical-design.md) và lộ trình trong [implementation-plan.md](implementation-plan.md).

---

## 1. Mục tiêu và Công cụ Kiểm thử

| Hạng mục | Lựa chọn kỹ thuật | Mục tiêu cụ thể |
| --- | --- | --- |
| **Unit Test Framework** | **Unity** (`unity.c`, `unity.h` nhúng trực tiếp vào `tests/`) kết hợp `<assert.h>` | Nhẹ, chuẩn C, không phụ thuộc cài đặt thư viện hệ thống |
| **Đo Code Coverage** | `gcc --coverage` (`-fprofile-arcs -ftest-coverage`) + `gcov` / `lcov` / `gcovr` | **Line Coverage > 90%** và **Function Coverage > 90%** trên toàn bộ module lõi |
| **Cô lập CSDL khi Test** | SQLite In-Memory (`":memory:"`) | Chạy hàng trăm test case trong vài mili-giây, không để lại file DB rác |
| **Load / Stress Test** | Công cụ sinh tải tự động (`tests/load_test`) giả lập nhiều Bot Client qua TCP | Đo thông lượng, độ trễ, tính ổn định của `select()` và giao dịch SQLite dưới tải cao |

---

## 2. Thiết kế Unit Test

### 2.1. Nguyên tắc thiết kế mã nguồn phục vụ Testability (Khả năng kiểm thử)

Để đạt độ bao phủ **> 90%** mà không bị cản trở bởi vòng lặp `while(1)` của server hay thao tác nhập xuất terminal của client, mã nguồn trong `src/` tuân thủ 4 nguyên tắc:

1. **Giữ `main.c` tối giản:**
   - `src/server/main.c` và `src/client/main.c` chỉ đọc tham số dòng lệnh, khởi tạo tài nguyên và gọi vòng lặp chính. Toàn bộ logic xử lý nằm ở các module độc lập.
2. **Tách biệt giao thức và xử lý luồng byte khỏi socket thật:**
   - Module `src/common/protocol.c` nhận bộ đệm byte (`uint8_t *buf, size_t len`) thay vì gọi trực tiếp `recv()`/`send()`. Nhờ đó có thể truyền mọi kịch bản cắt/gộp gói TCP vào bộ đệm trong Unit Test.
3. **Truyền đường dẫn DB động vào Storage:**
   - Hàm khởi tạo `storage_init(const char *db_path)` nhận tham số đường dẫn. Khi chạy thật truyền file `"chess.db"`, khi chạy Unit Test truyền `":memory:"`.
4. **Thực thi vòng lặp mạng theo từng bước (`network_poll`):**
   - Tách một chu kỳ `select()` thành hàm `network_poll(server_ctx, timeout_ms)` để Unit Test và Integration Test có thể điều khiển từng bước sự kiện mà không bị treo trong vòng lặp vô hạn.

### 2.2. Danh mục Unit Test theo từng Module và Yêu cầu

| File Test | Module kiểm thử (`src/`) | Đối chiếu yêu cầu | Các kịch bản kiểm thử trọng tâm (Test Cases) | Chỉ tiêu Coverage |
| --- | --- | --- | --- | --- |
| `tests/test_protocol.c` | `common/protocol.c` | Xử lý truyền dòng (1đ), `P-11` | - Đóng khung `[4 byte length][JSON UTF-8]` hợp lệ.<br>- **TCP Fragmentation:** Nhận 1–2 byte độ dài trước, lần sau mới nhận đủ khung.<br>- **TCP Coalescing:** Nhận 3 khung dính liền trong 1 lần đọc buffer.<br>- Khung lỗi: `N = 0`, `N > 65536`, JSON hỏng, thiếu trường bắt buộc (`type`, `request_id`). | **≥ 95%** |
| `tests/test_chess.c` | `chess/chess.c` | Kiểm tra nước đi hợp lệ (1đ), Xác định kết quả (1đ), `FR-08`, `P-07` | - Khởi tạo bàn cờ chuẩn (`chess_init`).<br>- Nước đi hợp lệ/bất hợp lệ của từng quân (Tốt, Mã, Tượng, Xe, Hậu, Vua), cản đường, bắt quân.<br>- Luật an toàn Vua: cấm đi vào ô bị chiếu, bắt buộc đỡ/tránh chiếu.<br>- Nước đặc biệt: Nhập thành (`O-O`, `O-O-O`) và các điều kiện cấm nhập thành; Bắt tốt qua đường (*En passant*); Phong cấp (`q`, `r`, `b`, `n`).<br>- Kết thúc ván: Chiếu hết (*Checkmate*), Hết nước đi (*Stalemate*), Lặp vị trí 3 lần, Quy tắc 50 nước (100 ply), Hòa do thiếu quân chiếu hết. | **≥ 95%** |
| `tests/test_storage.c` | `server/storage.c` | Đăng ký/tài khoản (2đ), Ghi log (1đ), `FR-01`, `FR-03`, `FR-09`, `P-01`, `P-08` | - Khởi tạo 3 bảng `users`, `matches`, `moves` trên SQLite `":memory:"`.<br>- Tạo user, kiểm tra trùng `username`, cập nhật điểm thắng `+3`, hòa `0`, thua `-3` (bao gồm điểm âm).<br>- Truy vấn bảng xếp hạng theo `score DESC, user_id ASC` (`P-01`).<br>- Giao dịch nguyên tử (`storage_finalize_match`): ghi nước đi cuối + kết quả + cộng/trừ điểm; chống cộng điểm 2 lần cho cùng 1 trận.<br>- Xuất nội dung file log UTF-8 (`storage_export_log`) chứa thời gian bắt đầu/kết thúc, IP hai đối thủ và danh sách nước đi. | **≥ 90%** |
| `tests/test_account.c` | `server/account.c` | Đăng ký (2đ), Đăng nhập & quản lý phiên (2đ), `FR-01`, `FR-02`, `P-02`, `P-10` | - Đăng ký tài khoản hợp lệ, từ chối username/password sai độ dài hoặc ký tự.<br>- Băm mật khẩu có salt và xác thực mật khẩu khi đăng nhập.<br>- Quản lý phiên: chặn đăng nhập trùng tài khoản đang online (`ALREADY_ONLINE`), chặn gọi lệnh khi chưa đăng nhập (`AUTH_REQUIRED`).<br>- Đăng xuất ngoài trận thành công; từ chối đăng xuất khi đang trong trận (`IN_MATCH`). | **≥ 90%** |
| `tests/test_lobby.c` | `server/lobby.c` | DS người chơi sẵn sàng (2đ), Thách đấu (2đ), Nhận/Từ chối (1đ), `FR-04..06`, `P-01`, `P-03` | - `lobby_list_ready`: chỉ liệt kê user `ONLINE_IDLE`, loại trừ user offline hoặc `IN_MATCH`; hiển thị đúng cờ `has_pending_challenge`.<br>- Kiểm tra giới hạn hạng khi thách đấu: hiệu số bậc `<= 10` hợp lệ (biên = 10), hiệu số bậc `>= 11` bị từ chối với mã `RANK_DISTANCE`.<br>- Chặn tự thách đấu chính mình hoặc thách đấu người đang bận (`BUSY`).<br>- Phản hồi thách đấu: chấp nhận, từ chối, hết hạn sau 60 giây (`CHALLENGE_EXPIRED`), hủy lời mời khi 1 bên rớt mạng hoặc đổi hạng vượt quá 10 bậc lúc chấp nhận. | **≥ 90%** |
| `tests/test_match.c` | `server/match.c` | Chuyển nước đi (2đ), Kết quả & log (2đ), Xin ngừng ván (1đ), `FR-07..11`, `P-04..08` | - Tạo trận mới sau khi chấp nhận lời mời; người thách đấu cầm quân Trắng (`P-04`).<br>- Gửi nước đi đúng lượt, đúng `expected_ply`; từ chối khi sai lượt (`NOT_YOUR_TURN`), sai `expected_ply` (`STALE_STATE`) hoặc nước đi phạm luật (`ILLEGAL_MOVE`).<br>- Xin ngừng ván (`RESIGN`): xử thua người xin ngừng (`-3` điểm), đối thủ thắng (`+3` điểm), trả cả hai về trạng thái `ONLINE_IDLE` để có thể chơi ván mới.<br>- Xử lý mất kết nối giữa trận (`match_on_disconnect` theo `P-05`): bên mất kết nối bị xử thua, trận quyết toán đúng 1 lần.<br>- Tạo luồng khung gửi log (`LOG_BEGIN`, `LOG_CHUNK`, `LOG_END`). | **≥ 90%** |
| `tests/test_network_cli.c` | `server/network.c`, `client/cli.c` | Cơ chế vào/ra socket (2đ), `P-09` | - Kiểm thử `network_poll` với socket loopback / `socketpair`: nhận kết nối mới, đọc/ghi không chặn, xử lý EOF khi client đóng kết nối, giới hạn hàng đợi gửi.<br>- Kiểm thử bộ phân tích cú pháp lệnh CLI (`cli_handle_input`) và xử lý sự kiện từ server (`cli_handle_event`) lưu file log tạm và hoàn tất khi nhận `LOG_END`. | **≥ 85%** |

---

## 3. Thiết kế Bài kiểm thử khả năng chịu tải (Load / Stress Test)

### 3.1. Kiến trúc công cụ sinh tải (`tests/load_test`)

Công cụ kiểm thử tải đóng vai trò bộ điều phối nhiều **Bot Client ảo** kết nối đồng thời tới Server qua TCP Socket, sử dụng đúng định dạng khung `[4 byte độ dài][JSON UTF-8]` của dự án.

- **Tối ưu cấu hình SQLite trên Server khi chịu tải:**
  - Bật chế độ **`PRAGMA journal_mode=WAL;`** (Write-Ahead Logging) và **`PRAGMA synchronous=NORMAL;`** khi khởi tạo SQLite để giảm thiểu thời gian khóa đĩa khi nhiều trận đấu cùng ghi nước đi và quyết toán điểm song song.

### 3.2. Các kịch bản chịu tải (Load & Stress Scenarios)

#### Kịch bản 1 — Tải kết nối đồng thời và Sảnh chờ (Concurrent Connections & Lobby Stress)
- **Cách thực hiện:** Mở đồng loạt $N$ kết nối TCP ($N = 50, 200, 500$, tiến sát giới hạn `FD_SETSIZE` của `select()`), thực hiện đăng ký/đăng nhập và gửi yêu cầu `LIST_READY` lặp lại.
- **Mục đích kiểm chứng:**
  - Đánh giá khả năng xử lý của vòng lặp `select()` với non-blocking socket.
  - Kiểm tra cơ chế từ chối/đóng kết nối an toàn khi số descriptor đạt ngưỡng giới hạn của `FD_SETSIZE` mà không làm sập (crash) server.

#### Kịch bản 2 — Nhiều trận đấu diễn ra song song (Parallel Matches Throughput)
- **Cách thực hiện:** Khởi tạo $2M$ Bot Client ($M = 25, 50, 100, 200$ cặp đấu song song, tương ứng $50 \to 400$ người chơi). Các cặp tự động gửi lời thách đấu, chấp nhận, và tuần tự gửi các nước đi của một ván cờ kịch bản (ví dụ: Scholar's Mate hoặc ván cờ 30–40 ply) cho tới khi kết thúc ván, nhận `MATCH_RESULT` và tải xong toàn bộ các chunk của file log (`LOG_BEGIN` $\to$ `LOG_END`).
- **Mục đích kiểm chứng:**
  - Đo thông lượng xử lý nước đi (**Moves/sec**) và thông điệp (**Messages/sec**).
  - Kiểm tra tính cô lập trạng thái giữa các bàn cờ: không có nước đi nào bị gửi nhầm bàn.
  - Đối chiếu tổng điểm trong SQLite sau khi $M$ trận kết thúc: tổng độ lệch điểm của toàn hệ thống phải bằng $0$ (mỗi trận thắng/thua cộng $+3$ và trừ $-3$, trận hòa cộng $0$), mọi trận đều xuất đúng file log.

#### Kịch bản 3 — Kiểm thử độ bền dưới điều kiện mạng bất thường (Fault & Slow-Client Torture Test)
- **Cách thực hiện:** Trong khi 50 trận đấu đang diễn ra bình thường, cho một nhóm Bot gây nhiễu:
  1. **Fragmented Sender:** Gửi khung JSON bị cắt nhỏ từng byte cách nhau vài mili-giây.
  2. **Slow Reader:** Ngừng đọc socket khi server đang truyền các khối `LOG_CHUNK` để làm đầy hàng đợi gửi của kết nối đó.
  3. **Abrupt Disconnector:** Đóng socket đột ngột (`RST`/`EOF`) ngay giữa lượt đi hoặc ngay khi đang chờ lời thách đấu.
- **Mục đích kiểm chứng:**
  - Chứng minh một client chậm hoặc gửi dữ liệu nhỏ giọt **không làm nghẽn (block)** các trận đấu của các client khác.
  - Chứng minh server không bị lỗi `SIGPIPE`, không rò rỉ bộ nhớ (memory leak) và xử thua đúng cho bên ngắt kết nối đột ngột theo `P-05`.

### 3.3. Bộ chỉ số đo đạc (Metrics)

Sau mỗi lần chạy `load_test`, công cụ xuất báo cáo thống kê các chỉ số sau:

| Nhóm chỉ số | Chỉ số cụ thể | Ý nghĩa |
| --- | --- | --- |
| **Quy mô tải** | Số kết nối đồng thời ($N$), Số trận song song ($M$) | Xác định ngưỡng hoạt động ổn định và giới hạn `FD_SETSIZE` của `select()` |
| **Thông lượng (Throughput)** | Số thông điệp/giây (`msg/s`), Số nước đi/giây (`moves/s`) | Năng lực xử lý của Server + Chess Engine + SQLite |
| **Độ trễ (Latency)** | Độ trễ phản hồi trung bình (`Avg`), `p95`, `p99` (ms) cho mỗi lệnh `MOVE` và `LIST_READY` | Đánh giá trải nghiệm thời gian thực của người chơi |
| **Độ tin cậy & Toàn vẹn** | Tỷ lệ thành công (%), Số lỗi giao thức/timeout, Kiểm tra tổng điểm ($\sum \Delta \text{score} = 0$) | Chứng minh hệ thống không sai lệch dữ liệu dưới tải cao |
| **Tài nguyên hệ thống** | Mức sử dụng CPU (%), RAM đỉnh (MB), Kiểm tra rò rỉ bộ nhớ (`valgrind` / AddressSanitizer) | Chứng minh quản lý bộ đệm và phiên trong C an toàn |

---

## 4. Cấu trúc thư mục Kiểm thử dự kiến

Khi bước vào giai đoạn lập trình, thư mục `tests/` sẽ được tổ chức như sau:

```text
tests/
  unity/
    unity.c / unity.h             # Framework Unit Test nhẹ cho C
  unit/
    test_protocol.c               # Unit test đóng/tách khung và JSON
    test_chess.c                  # Unit test luật cờ vua và điều kiện kết thúc
    test_storage.c                # Unit test SQLite (:memory:), hạng, điểm và xuất log
    test_account.c                # Unit test đăng ký, đăng nhập, quản lý phiên
    test_lobby.c                  # Unit test danh sách sẵn sàng và thách đấu (<= 10 bậc)
    test_match.c                  # Unit test luồng trận đấu, nước đi, RESIGN, disconnect
    test_network_cli.c            # Unit test vòng lặp network_poll và bộ phân tích lệnh CLI
  load/
    load_test.c                   # Công cụ sinh tải đa kết nối và đo thông lượng/độ trễ
  scripts/
    run_coverage.sh               # Script chạy toàn bộ unit test và xuất báo cáo gcov/lcov (>90%)
```

---

## 5. Quy trình kết hợp Viết Code – Viết Test – Commit hàng tuần

Áp dụng quy tắc **"Code module nào đi kèm Unit Test của module đó trong cùng tuần"**:

| Tuần | Mốc thời gian | Trọng tâm Phát triển (`src/`) | Trọng tâm Kiểm thử (`tests/`) | Chỉ tiêu Commit tối thiểu |
| --- | --- | --- | --- | --- |
| **Tuần 1** | 10/11 – 16/11 | Chốt thiết kế, khởi tạo khung dự án | Chuẩn bị kế hoạch kiểm thử | Khởi tạo repo & tài liệu |
| **Tuần 2** | 17/11 – 23/11 | `protocol.c`, `chess.c` (bàn cờ & nước đi cơ bản), `storage.c` (khởi tạo bảng & user), cấu hình build + `Unity` | `test_protocol.c`, phần đầu `test_chess.c` và `test_storage.c` | $\ge 5$ commit, Coverage các module đã viết $> 90\%$ |
| **Tuần 3** | 24/11 – 30/11 | `network.c` (`select()`), `account.c` (đăng ký/đăng nhập/phiên), `lobby.c` (danh sách sẵn sàng, thách đấu $\le 10$ bậc) | `test_account.c`, `test_lobby.c`, mở rộng `test_storage.c` (xếp hạng) | $\ge 5$ commit, Coverage tích lũy $> 90\%$ |
| **Tuần 4** | 01/12 – 07/12 | Hoàn thiện luật đặc biệt trong `chess.c` (nhập thành, bắt tốt qua đường, phong cấp, chiếu hết, hòa) và `match.c` (điều phối trận, `RESIGN`, disconnect) | Hoàn thiện `test_chess.c` và `test_match.c` | $\ge 5$ commit, Coverage tích lũy $> 90\%$ |
| **Tuần 5** | 08/12 – 14/12 | Xuất và truyền file log theo chunk, lịch sử trận, hoàn thiện `client/cli.c` | `test_network_cli.c`, xây dựng công cụ `load_test` (Kịch bản 1 & 2) | $\ge 5$ commit, Coverage toàn bộ $> 90\%$ |
| **Tuần 6** | 15/12 – 21/12 | Chạy Load/Stress Test (Kịch bản 1, 2, 3), tối ưu hiệu năng, sửa lỗi, đóng gói dự án | Xuất báo cáo `lcov` cuối cùng ($> 90\%$) và bảng số liệu chịu tải đưa vào báo cáo/slide | $\ge 5$ commit, sẵn sàng bảo vệ |
