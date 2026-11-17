# Thiết kế kỹ thuật

**Trạng thái:** thiết kế dự kiến, chưa có mã nguồn. Các lựa chọn P-01 đến P-11 được định nghĩa trong [yêu cầu](requirements.md) và đều là **Đề xuất thiết kế** cần duyệt. Tên hàm, kiểu dữ liệu, giao thức và cây thư mục bên dưới là phương án tham chiếu, chưa phải API hoặc file đã tồn tại.

## 1. Kiến trúc Client–Server

Client là chương trình CLI của người chơi; server là chương trình nhận kết nối và điều phối trận. Mỗi client duy trì một kết nối TCP tới server. Nhiều trận đồng thời có nghĩa server lưu và xử lý nhiều trạng thái trận trong cùng vòng lặp, không bắt buộc tạo một luồng hay một tiến trình cho mỗi trận.

```text
Client CLI A ── TCP ──┐
Client CLI B ── TCP ──┤   Server: select() → Network → module nghiệp vụ
Client CLI C ── TCP ──┤                                  │
Client CLI D ── TCP ──┘                           SQLite / dữ liệu log
```

| Bên | Trách nhiệm |
| --- | --- |
| Client | Đọc lệnh, hiển thị bàn cờ/sảnh/kết quả, gửi yêu cầu, nhận sự kiện bất đồng bộ và lưu file log nhận được |
| Server | Xác thực phiên, duy trì sảnh/lời mời/trận, kiểm tra quyền và luật cờ, lưu dữ liệu, tính điểm, gửi kết quả và log |
| SQLite | Lưu tài khoản, điểm, thông tin trận và các nước đi đã được server chấp nhận |

Client chỉ hiển thị nước đi sau khi server xác nhận; không gửi bàn cờ hoặc kết quả làm nguồn dữ liệu chính. Bất đồng bộ nghĩa là client có thể nhận lời mời hoặc nước đi đối phương ngay cả khi người dùng chưa nhập lệnh tiếp theo.

### Vòng lặp server — Đề xuất thiết kế P-09/P-11

1. Tạo tập socket cần đọc và tập socket có dữ liệu chờ gửi; đặt timeout theo lời mời sắp hết hạn.
2. Gọi `select()` để chờ kết nối mới, dữ liệu đến, khả năng ghi hoặc timeout.
3. Nhận kết nối; với socket có dữ liệu, đọc vào bộ đệm, tách mọi khung hoàn chỉnh rồi phân phối yêu cầu.
4. Xử lý yêu cầu tuần tự, kiểm tra lời mời hết hạn, cập nhật trạng thái; xếp phản hồi vào hàng đợi gửi.
5. Gửi phần dữ liệu mà socket đang nhận được; giữ phần còn lại cho vòng sau.

Socket không chặn (nonblocking) trả quyền điều khiển ngay nếu chưa đọc/ghi được, tránh một client chậm giữ toàn bộ server. `EAGAIN`/`EWOULDBLOCK` báo thử lại sau, không phải mất kết nối. `recv()` trả 0 là đầu bên kia đã đóng luồng gửi (EOF); lỗi nghiêm trọng cũng dẫn tới xử lý ngắt kết nối theo P-05.

`select()` bị giới hạn bởi `FD_SETSIZE` của môi trường; phải kiểm tra giá trị mô tả socket (file descriptor) trước khi đưa vào tập, không chỉ đếm kết nối. Khi hết khả năng tiếp nhận, đóng kết nối mới có kiểm soát. Mỗi vòng chỉ xử lý một lượng hữu hạn dữ liệu từ một client để các trận khác vẫn tiến triển. Tác vụ SQLite giữ ngắn; tránh gửi cả file bằng một vòng lặp chặn.

## 2. Module và quan hệ

Module là nhóm hàm cùng trách nhiệm; không cần một hệ thống lớp hay khung mở rộng phức tạp.

| Module | Trách nhiệm | Quan hệ chính |
| --- | --- | --- |
| Network | `select()`, socket, bộ đệm đọc/ghi, đóng khung, đọc JSON và chuyển lệnh | Chuyển yêu cầu tới Account/Session, Lobby/Challenge hoặc Match; gửi sự kiện trả về |
| Account/Session | Đăng ký, kiểm tra mật khẩu, gắn tài khoản vào kết nối, đăng xuất | Dùng Storage/Logging; cung cấp danh tính cho Lobby/Challenge và Match |
| Lobby/Challenge | Danh sách sẵn sàng, thứ hạng, vòng đời lời mời, kiểm tra điều kiện thách đấu | Đọc phiên và thứ hạng; gọi Match khi chấp nhận hợp lệ |
| Match | Quản lý trận, màu quân, lượt, xin ngừng, mất kết nối và quyết toán | Gọi Chess Engine và Storage/Logging; tạo thông báo cho Network |
| Chess Engine | Bàn cờ, kiểm tra/thực hiện nước đi, chiếu, kết thúc và dữ liệu lặp vị trí | Không phụ thuộc socket, CLI hoặc SQLite; trả kết quả cho Match |
| Storage/Logging | Tài khoản, thứ hạng, giao dịch ghi trận/điểm/nước đi, lịch sử và xuất log | Được các module nghiệp vụ gọi; dùng SQLite |
| Client CLI | Nhập lệnh, vẽ bàn cờ dạng ký tự, hiển thị sự kiện, nhận file log | Dùng phần đóng khung giao thức chung; không truy cập SQLite của server |

Network không quyết định thắng/thua. Chess Engine không tự cộng điểm. Match là nơi điều phối kết thúc trận và trả hai người về sảnh. Server có thể dùng một bộ phân phối lệnh trực tiếp bằng loại thông điệp, chưa cần thêm tầng dịch vụ.

## 3. API nội bộ dự kiến

API (Application Programming Interface) ở đây là cách các module gọi hàm của nhau. Các tên kiểu như `Session`, `Match`, `GameState` chỉ mô tả dữ liệu dự kiến, chưa phải khai báo header C. Hàm có thể trả mã lỗi kèm dữ liệu kết quả; không dùng con trỏ rỗng để biểu diễn mọi loại lỗi.

| Module / hàm | Mục đích | Đầu vào chính | Đầu ra chính |
| --- | --- | --- | --- |
| Network — `network_poll` | Thực hiện một vòng chờ/đọc/ghi | Danh sách kết nối, hạn chờ | Sự kiện nhận lệnh/ngắt kết nối |
| Network — `protocol_next_frame` | Lấy một thông điệp đầy đủ từ bộ đệm | Bộ đệm byte | Thông điệp, cần thêm dữ liệu hoặc lỗi khung |
| Network — `network_queue_message` | Xếp khung vào hàng đợi gửi | Kết nối, nội dung JSON | Thành công hoặc lỗi giới hạn hàng đợi |
| Account/Session — `account_register` | Tạo tài khoản và lưu mật khẩu đã băm | Tên đăng nhập, mật khẩu | `user_id` hoặc lỗi dữ liệu/trùng tên/lưu trữ |
| Account/Session — `session_login` | Xác thực và tạo phiên | Kết nối, tên đăng nhập, mật khẩu | `Session` hoặc lỗi đăng nhập/đã online |
| Account/Session — `session_logout` | Hủy phiên và lời mời liên quan | `Session` | Thành công hoặc lỗi đang trong trận |
| Lobby/Challenge — `lobby_list_ready` | Lấy người online chưa vào trận | Người yêu cầu, giới hạn phân trang | Người chơi, điểm, hạng, cờ lời mời và trang tiếp |
| Lobby/Challenge — `challenge_create` | Kiểm tra và tạo lời mời | Phiên người gửi, `target_user_id` | `Challenge` hoặc lỗi khoảng cách/bận |
| Lobby/Challenge — `challenge_respond` | Chấp nhận/từ chối, kiểm tra lại điều kiện | Phiên người nhận, `challenge_id`, quyết định | Trạng thái lời mời, `match_id` nếu tạo trận |
| Match — `match_create` | Khởi tạo trận sau khi chấp nhận | Hai người, IP, màu quân | Trận đã lưu hoặc lỗi; lỗi không chiếm hai người |
| Match — `match_submit_move` | Kiểm tra quyền/lượt và điều phối nước đi | Phiên, `match_id`, `expected_ply`, nước đi | Trạng thái mới hoặc lý do từ chối |
| Match — `match_finish` | Kết thúc một lần, tính điểm, mở lại sảnh, phát sự kiện | Trận, kết quả, nguyên nhân | Kết quả đã lưu hoặc lỗi lưu trữ |
| Match — `match_on_disconnect` | Xử lý mất kết nối theo P-05 | Phiên, thời điểm phát hiện | Quyết toán trận nếu có; sự kiện dọn phiên/lời mời |
| Chess Engine — `chess_init` | Tạo thế xuất phát | Không có | `GameState` ban đầu |
| Chess Engine — `chess_try_move` | Kiểm tra và tạo trạng thái tiếp theo | Trạng thái hiện tại, nước đi | Bản trạng thái mới hoặc lý do sai luật; không sửa bản gốc khi lỗi |
| Chess Engine — `chess_evaluate` | Xác định tiếp tục/chiếu/thắng/hòa | Trạng thái sau nước đi và lịch sử vị trí | Trạng thái ván và nguyên nhân |
| Storage/Logging — `storage_get_ranking` | Lấy thứ hạng P-01 | Tập tài khoản / mã người cần tra | Điểm và vị trí xếp hạng |
| Storage/Logging — `storage_append_move` | Ghi nước đi đã kiểm tra | Trận, `ply`, nước đi, thời điểm | Thành công hoặc lỗi giao dịch |
| Storage/Logging — `storage_finalize_match` | Lưu kết quả và thay đổi điểm nguyên tử | Trận còn hoạt động, kết quả; nước cuối nếu có | Bản ghi đã chốt hoặc lỗi; không tính điểm lặp |
| Storage/Logging — `storage_export_log` | Dựng luồng byte file log từ dữ liệu bền vững | Phiên yêu cầu, `match_id` | Nội dung log UTF-8 hoặc lỗi quyền/chưa kết thúc |
| Client CLI — `cli_handle_input` | Chuyển lệnh terminal thành yêu cầu | Dòng nhập, trạng thái client | Yêu cầu hoặc thông báo cú pháp |
| Client CLI — `cli_handle_event` | Cập nhật hiển thị và lưu log | Thông điệp server, trạng thái client | Bàn cờ/sảnh/kết quả/file nhận được |

Đăng xuất trong trận bị từ chối theo **Đề xuất thiết kế P-02/P-05**: người chơi dùng `RESIGN` trước, hoặc chịu xử lý mất kết nối nếu đóng client. Server vẫn tự xử lý ngắt kết nối, không phụ thuộc client gửi lệnh đăng xuất.

## 4. Dữ liệu chính và SQLite

### Dữ liệu trong bộ nhớ

| Dữ liệu | Trường quan trọng | Ý nghĩa |
| --- | --- | --- |
| User | `user_id`, `username`, `score` | Danh tính và điểm; không đưa dữ liệu mật khẩu ra sảnh |
| Session | `connection_id`, socket, `user_id`, trạng thái, bộ đệm vào/ra | Một phiên gắn với một TCP socket; `connection_id` do server cấp để tránh nhầm socket được hệ điều hành tái sử dụng |
| Challenge | `challenge_id`, người gửi/nhận, thời điểm tạo/hết hạn, trạng thái | `PENDING`, `ACCEPTED`, `DECLINED`, `EXPIRED`, `CANCELLED`; chỉ chuyển khỏi `PENDING` một lần |
| Match | `match_id`, hai người và màu, IP, thời gian, trạng thái, `GameState`, `ply` | `ACTIVE`, `FINISHED`, `ABORTED`; giữ trận kết thúc ở trạng thái chờ ghi nếu SQLite tạm lỗi |
| GameState | 64 ô, bên tới lượt, quyền nhập thành, ô bắt tốt qua đường, bộ đếm 50 nước, lịch sử khóa vị trí | Đủ dữ liệu để xét luật; chỉ 64 ô chưa đủ cho nhập thành/lặp vị trí |
| Move | `ply`, ô đi/đến, phong cấp, cờ bắt quân/nhập thành/bắt tốt qua đường, thời điểm | `ply` là số nửa nước: mỗi lần một bên đi tăng 1 |

Phiên có thể biểu diễn `UNAUTHENTICATED`, `ONLINE_IDLE`, `IN_MATCH`; offline là không có phiên. Lời mời đang chờ là thuộc tính riêng, không biến người chưa vào trận thành người “đang chơi”. Vì vậy danh sách sẵn sàng theo FR-04 vẫn có người đang chờ lời mời, kèm `has_pending_challenge`.

### Các bảng SQLite đề xuất — chưa có SQL

| Bảng | Trường quan trọng | Ràng buộc / mục đích |
| --- | --- | --- |
| `users` | `user_id`, `username`, `password_hash`, `score`, `created_at` | Khóa chính `user_id`; `username` duy nhất; hash chứa salt/tham số cần xác thực; điểm ban đầu 0 theo P-01 |
| `matches` | `match_id`, `white_user_id`, `black_user_id`, `white_ip`, `black_ip`, `started_at`, `ended_at`, `status`, `result`, `reason`, `white_score_delta`, `black_score_delta` | Hai người khác nhau, tham chiếu `users`; trận chưa kết thúc có kết quả/thời gian kết thúc rỗng |
| `moves` | `match_id`, `ply`, `from_square`, `to_square`, `promotion`, `move_flags`, `played_at` | Khóa ghép `(match_id, ply)`; tham chiếu `matches`; chỉ lưu nước hợp lệ, thứ tự liên tiếp |

Khóa chính nhận diện duy nhất một bản ghi; khóa ngoại liên kết bản ghi giữa các bảng. Chỉ có ba bảng cho bản đầu: phiên và lời mời nằm trong bộ nhớ, không phục hồi khi server chạy lại theo P-06. Thứ hạng tính từ `users`, không lưu thêm một hạng dễ bị cũ.

Theo **Đề xuất thiết kế P-10**, tên đăng nhập dài 3–32 ký tự ASCII (bộ ký tự cơ bản, ở đây chỉ nhận `A-Z`, `a-z`, `0-9`, `_`), phân biệt hoa thường; mật khẩu 8–128 byte UTF-8. UTF-8 là cách mã hóa văn bản hỗ trợ tiếng Việt, trong đó một ký tự có thể chiếm nhiều byte. Không ghi mật khẩu hoặc toàn bộ yêu cầu đăng nhập vào log. Kiểm tra độ dài ở server và dùng câu lệnh SQLite có tham số ràng buộc, không ghép chuỗi đầu vào vào truy vấn.

### Nhất quán khi ghi dữ liệu — Đề xuất thiết kế P-11

Giao dịch (transaction) là nhóm thao tác SQLite cùng thành công hoặc cùng hủy. Khi nhận nước đi, Match tạo trạng thái kế tiếp bằng Chess Engine, chưa công bố ngay:

- Nếu ván còn tiếp tục: ghi nước đi thành công rồi mới thay trạng thái trong bộ nhớ và gửi `MOVE_APPLIED`.
- Nếu nước đi kết thúc ván: ghi nước cuối, kết quả, thời gian kết thúc, hai thay đổi điểm trong cùng một giao dịch; sau đó mới công bố trạng thái cuối.
- Nếu đầu hàng/mất kết nối: giao dịch chốt kết quả và điểm, không tạo nước đi giả.
- Chỉ cho phép quyết toán bản ghi đang `ACTIVE`. Yêu cầu kết thúc lặp lại không được tính điểm lần hai.
- Nếu lỗi trước khi ghi một nước thường: giữ nguyên bàn cờ và báo lỗi. Nếu lỗi khi chốt kết quả: giữ trận khóa ở trạng thái chờ ghi, thử lại giao dịch; chưa gửi kết quả thành công và chưa cho hai bên vào trận mới.

Nhờ vòng lặp xử lý tuần tự, hai lời chấp nhận hoặc hai yêu cầu kết thúc không sửa cùng trạng thái đồng thời; vẫn phải kiểm tra trạng thái trước mỗi thao tác. Khi khởi động lại, đánh dấu mọi trận còn `ACTIVE` là `ABORTED`, đặt thời điểm phát hiện sự cố làm `ended_at` và không đổi điểm theo P-06. Log ghi rõ thời điểm đó không phải thời điểm biết chính xác server dừng.

### Log trận — Đề xuất thiết kế P-08

SQLite là nguồn dựng log, không cần duy trì thêm bản sao file riêng trên server. File xuất là văn bản UTF-8 `match_<match_id>.txt`, chứa mã trận, tên/mã và màu hai người, IP lấy từ kết nối tại lúc bắt đầu, thời gian UTC bắt đầu/kết thúc, kết quả/nguyên nhân và các nước theo `ply`. UTC là múi giờ chuẩn để hai client diễn giải thời gian nhất quán.

Mỗi nước được biểu diễn ngắn như `1. e2e4`, `2. e7e5`, `3. g1f3`; số ở đây là `ply`, không phải số lượt đầy đủ trong ký pháp cờ. Phong cấp ghi như `e7e8q`. Không bắt buộc xuất ký pháp giải đấu hoặc PGN trong bản đầu.

## 5. Giao thức client–server

Toàn bộ định dạng cụ thể trong mục này là **Đề xuất thiết kế P-11**. JSON biểu diễn nội dung; TCP chỉ vận chuyển byte, không biết ranh giới thông điệp.

### Đóng khung và bộ đệm

Khung (frame) là một thông điệp có ranh giới xác định:

```text
[4 byte độ dài N, số nguyên không dấu, network byte order][N byte JSON UTF-8]
```

Network byte order là thứ tự byte từ phần có trọng số lớn tới nhỏ. N chỉ tính số byte của JSON, không tính 4 byte đầu hoặc byte kết thúc chuỗi C. Giới hạn đề xuất: `1 <= N <= 65536` byte. Cần kiểm tra giới hạn trước cấp phát bộ nhớ và kiểm tra kiểu/trường JSON sau khi giải mã.

Không giả định một lần `recv()` trả một thông điệp: lần đầu có thể chỉ có 2 byte đầu, lần sau có phần còn lại của khung và cả khung tiếp theo. Server/client tích lũy byte, chờ đủ 4 byte để biết N, chờ đủ `4 + N` byte để lấy một khung, rồi lặp và giữ lại phần dư. `send()` cũng có thể chỉ gửi một phần; phải giữ vị trí gửi và tiếp tục sau.

Độ dài sai giới hạn làm đóng kết nối. Khung đủ độ dài nhưng JSON/lệnh không hợp lệ nhận `ERROR` nếu còn trả lời được. Hàng đợi gửi mỗi client giới hạn đề xuất 1 MiB; khi vượt giới hạn thì ngắt client chậm theo P-05. Log được tạo theo từng khối khi hàng đợi còn chỗ, không dồn toàn file vào hàng đợi. Danh sách sảnh/lịch sử phân trang tối đa 50 mục mỗi lần để giữ khung nhỏ.

### Loại lệnh và sự kiện

Yêu cầu có `type` và `request_id` là số tăng trong một kết nối. Phản hồi trực tiếp mang lại `request_id`; sự kiện tự phát như lời mời đến không cần trường này. `request_id` chỉ ghép yêu cầu/phản hồi, không phải cơ chế gửi lại an toàn. Client không tự gửi lại lệnh thay đổi trạng thái khi chưa rõ kết quả; tra lịch sử hoặc nhận lỗi trạng thái thay vì suy đoán thành công.

| Hướng | Loại | Nội dung chính |
| --- | --- | --- |
| Client → Server | `REGISTER`, `LOGIN`, `LOGOUT` | Tên/mật khẩu khi đăng ký/đăng nhập; logout hủy phiên nếu ngoài trận |
| Client → Server | `LIST_READY` | `after_user_id` làm mốc trang nếu có; danh sách theo `user_id` tăng dần, mỗi mục có thứ hạng riêng |
| Client → Server | `CHALLENGE`, `CHALLENGE_REPLY` | Người nhận hoặc mã lời mời và `accept` |
| Client → Server | `MOVE` | Mã trận, `expected_ply`, ô đi/đến, phong cấp nếu cần |
| Client → Server | `RESIGN` | Mã trận; người gửi chịu thua |
| Client → Server | `HISTORY`, `GET_LOG` | Lịch sử của chính mình phân trang theo `match_id` giảm dần bằng `before_match_id`; tải log bằng mã trận đã kết thúc |
| Server → Client | `OK`, `ERROR` | Phản hồi trực tiếp; lỗi gồm `code`, `message` và `request_id` nếu đọc được |
| Server → Client | `READY_LIST`, `HISTORY_LIST` | Một trang kết quả, mốc trang tiếp nếu còn |
| Server → Client | `CHALLENGE_RECEIVED`, `CHALLENGE_UPDATED` | Thông tin lời mời; chấp nhận/từ chối/hết hạn/hủy và nguyên nhân |
| Server → Client | `MATCH_STARTED` | Mã trận, hai người, màu quân, bàn cờ xuất phát và `ply=0` |
| Server → Client | `MOVE_APPLIED` | Mã trận, `ply` mới, nước đi, bàn cờ sau nước đi, bên tới lượt và trạng thái chiếu |
| Server → Client | `MATCH_RESULT` | Kết quả chung, nguyên nhân, thay đổi điểm hai bên, điểm sau trận, thông tin log |
| Server → Client | `LOG_BEGIN`, `LOG_CHUNK`, `LOG_END` | Siêu dữ liệu file, các khối nội dung tuần tự, dấu kết thúc |

Sau khi xử lý thành công một lệnh thay đổi trạng thái, server trả `OK` cho người gửi và phát các sự kiện liên quan cho các bên. `LOGIN` trả `OK` kèm người dùng/điểm/hạng; `REGISTER` thành công chưa tự tạo phiên. `LIST_READY` và `HISTORY` trả loại danh sách tương ứng; `GET_LOG` trả luồng log gắn `request_id`. Chỉ `REGISTER`/`LOGIN` được dùng trước xác thực.

Trong `MATCH_STARTED` và `MOVE_APPLIED`, trường `board` đề xuất là chuỗi 64 ký tự: từ `a8` đến `h8`, tiếp hàng 7 và kết thúc ở `h1`. `PNBRQK` lần lượt là Tốt/Mã/Tượng/Xe/Hậu/Vua Trắng, chữ thường là quân Đen, `.` là ô trống; `turn` nhận `WHITE` hoặc `BLACK`. Đây là dữ liệu hiển thị, không thay thế trạng thái luật đầy đủ trên server.

### Ví dụ ngắn

Các dòng sau là **phần JSON của các khung riêng biệt**, không phải giao thức phân tách bằng xuống dòng. Số mã và điểm chỉ minh họa.

```json
{"type":"LOGIN","request_id":1,"username":"an01","password":"example-only"}
{"type":"CHALLENGE","request_id":2,"target_user_id":12}
{"type":"CHALLENGE_REPLY","request_id":3,"challenge_id":41,"accept":true}
{"type":"MOVE","request_id":4,"match_id":90,"expected_ply":0,"from":"e2","to":"e4"}
{"type":"RESIGN","request_id":5,"match_id":90}
{"type":"MATCH_RESULT","match_id":90,"result":"BLACK_WIN","reason":"RESIGN","white_delta":-3,"black_delta":3,"white_score":-3,"black_score":3,"log_available":true}
{"type":"GET_LOG","request_id":6,"match_id":90}
```

`expected_ply` phải bằng số nửa nước đã được server chấp nhận trước nước đang gửi; giúp phát hiện client dùng bàn cờ cũ hoặc gửi lại. Phong cấp thêm `promotion` nhận `q`, `r`, `b`, `n` (Hậu, Xe, Tượng, Mã); thiếu hoặc thừa trường phong cấp không phù hợp đều bị từ chối. Nhập thành gửi nước đi của Vua, ví dụ `e1` → `g1`; Engine tự xử lý Xe. Ô cờ dùng cột `a`–`h`, hàng `1`–`8` theo góc nhìn chuẩn của bên Trắng.

Kết quả chung dùng `WHITE_WIN`, `BLACK_WIN`, `DRAW`, `ABORTED`; nguyên nhân dùng `CHECKMATE`, `STALEMATE`, `REPETITION`, `FIFTY_MOVE`, `INSUFFICIENT_MATERIAL`, `RESIGN`, `DISCONNECT`, `SERVER_RESTART`. Không coi `ABORTED` là hòa. Các mã lỗi chính: `AUTH_REQUIRED`, `BAD_CREDENTIALS`, `ALREADY_ONLINE`, `INVALID_REQUEST`, `NOT_FOUND`, `FORBIDDEN`, `BUSY`, `RANK_DISTANCE`, `CHALLENGE_EXPIRED`, `NOT_YOUR_TURN`, `STALE_STATE`, `ILLEGAL_MOVE`, `MATCH_FINISHED`, `STORAGE_ERROR`.

### Gửi file log

Sau `MATCH_RESULT`, server chủ động gửi luồng log cho từng client còn kết nối, không đợi `GET_LOG`. Lệnh `GET_LOG` cho phép tải lại từ lịch sử. Mỗi client nhận tối đa một luồng log đang truyền; yêu cầu tải khác khi đó nhận `BUSY`. Nếu một trận mới kết thúc trong khi log cũ còn gửi, lượt tự gửi log mới chờ luồng cũ kết thúc, không trộn hai luồng.

```json
{"type":"LOG_BEGIN","match_id":90,"filename":"match_90.txt","total_bytes":240,"chunk_count":1}
{"type":"LOG_CHUNK","match_id":90,"seq":0,"data":"...nội dung log UTF-8..."}
{"type":"LOG_END","match_id":90,"total_bytes":240,"chunk_count":1}
```

`total_bytes` là số byte file sau giải mã JSON, không phải kích thước chuỗi JSON đã escape; 240 chỉ minh họa. Mỗi khối chứa tối đa 4096 byte UTF-8 trước escape, cắt tại ranh giới ký tự; `seq` bắt đầu 0. Các ký tự xuống dòng/dấu nháy trong `data` phải escape theo JSON. Thông điệp khác được xen giữa các khối log, nhưng từng luồng giữ đúng thứ tự. Khi tải lại theo yêu cầu, cả ba loại mang cùng `request_id` của `GET_LOG`.

Client ghi vào file tạm có tên tự tạo tại thư mục log của client; chỉ công bố file hoàn chỉnh khi nhận `LOG_END`, đủ số khối và số byte. Nếu đứt mạng, giữ trạng thái tải chưa hoàn tất và cho tải lại từ đầu. Chỉ hai người trong trận được lấy log hoặc xem trận đó qua `HISTORY`; không dùng đường dẫn do client gửi để đọc file server. Đề xuất lưu thư mục log tương lai không có nghĩa tạo thư mục đó trong lần viết tài liệu này.

## 6. Luồng hoạt động và trạng thái

1. **Kết nối và tài khoản:** client mở TCP; đăng ký nếu cần, rồi đăng nhập. Server xác thực, kiểm tra phiên trùng và gắn `user_id` vào kết nối.
2. **Vào sảnh:** client gọi `LIST_READY`. Server trả người online chưa vào trận, điểm, hạng và cờ đang có lời mời. Có thể hiển thị chính mình nhưng CLI không cho tự thách đấu; server luôn kiểm tra lại.
3. **Thách đấu:** server kiểm tra khác người, cả hai online/chưa chơi/chưa có lời mời, khoảng cách hạng không quá 10, rồi tạo lời mời 60 giây. Hai yêu cầu chéo xử lý tuần tự: lời mời hợp lệ đầu tiên được giữ, yêu cầu sau nhận `BUSY`.
4. **Phản hồi:** chỉ người nhận được trả lời. Từ chối/hết hạn thì đóng lời mời. Khi chấp nhận, server kiểm tra lại điều kiện và hạn trước khi tạo/lưu trận. Thành công chuyển cả hai sang `IN_MATCH`, phát `MATCH_STARTED`; người gửi cầm Trắng theo P-04. Nếu lưu trận thất bại thì không chuyển sang `IN_MATCH`, hủy lời mời và báo lỗi.
5. **Chơi cờ:** server kiểm tra người gửi thuộc trận, trận còn hoạt động, đúng lượt và đúng `expected_ply`; gọi Engine. Nước sai bị từ chối, không lưu như một nước hợp lệ. Nước đúng được lưu và gửi `MOVE_APPLIED` cho hai bên.
6. **Kết thúc:** khi Engine kết luận, có `RESIGN` hoặc mất kết nối theo P-05, Match quyết toán đúng một lần. Nếu nước cuối kết thúc trận, `MOVE_APPLIED` tới trước `MATCH_RESULT` trên từng kết nối. Sau giao dịch thành công, phiên còn kết nối về `ONLINE_IDLE` ngay, không đợi tải log xong.
7. **Kết quả và log:** server phát kết quả và tự gửi file log. Người đã ngắt kết nối nhận kết quả qua `HISTORY` rồi `GET_LOG` sau khi đăng nhập lại; đây là cách xử lý ngoại lệ theo P-08 cho FR-10.

Hết hạn lời mời dùng đồng hồ đơn điệu (monotonic clock, không bị ảnh hưởng khi chỉnh giờ hệ thống); kiểm tra hạn cả lúc nhận phản hồi, tránh chấp nhận muộn. Thời gian log dùng UTC. Rớt mạng khi có lời mời hủy lời mời; rớt mạng trong trận xử thua khi server phát hiện. Không có giới hạn thời gian suy nghĩ, nên một bên im lặng nhưng kết nối chưa lỗi không tự động bị xử thua.

## 7. Phạm vi Chess Engine

Engine chạy trên server và tự viết bằng C. Phạm vi nước đi cơ bản là để thực hiện FR-08; cách xử lý hòa và những giới hạn dưới đây là **Đề xuất thiết kế P-07**.

| Nhóm | Trách nhiệm dự kiến |
| --- | --- |
| Biểu diễn bàn cờ | 64 ô, loại/màu quân, bên tới lượt; phân biệt nước bình thường và nước đặc biệt |
| Nước đi từng quân | Tốt, Mã, Tượng, Xe, Hậu, Vua; hướng đi, cản đường, bắt quân, không bắt quân cùng màu |
| An toàn Vua | Xác định ô bị tấn công; không cho Vua đi vào chiếu hoặc nước đi để Vua mình còn bị chiếu; không cho bắt Vua |
| Nhập thành | Kiểm tra quyền chưa mất, Xe tương ứng còn ở ô gốc, đường trống; Vua không đang bị chiếu, không đi qua/kết thúc ở ô bị tấn công. Mất quyền không được phục hồi khi quân quay về |
| Bắt tốt qua đường | Chỉ ngay sau nước tốt đối phương đi hai ô; bỏ tốt bị bắt rồi kiểm tra lại an toàn Vua |
| Phong cấp | Tốt tới hàng cuối phải chọn Hậu/Xe/Tượng/Mã; hỗ trợ cả phong cấp khi bắt quân |
| Chiếu hết | Bên tới lượt đang bị chiếu và không có nước hợp lệ: thua |
| Hết nước hợp lệ | Bên tới lượt không bị chiếu nhưng không có nước hợp lệ: hòa (`STALEMATE`) |
| Lặp vị trí | Theo P-07: tự hòa khi cùng vị trí xuất hiện lần thứ 3, tính cả vị trí đầu. Khóa gồm quân trên các ô, bên tới lượt, quyền nhập thành và khả năng bắt tốt qua đường hợp lệ |
| Quy tắc 50 nước | Theo P-07: tự hòa khi đủ 100 nửa nước liên tiếp không đi tốt và không bắt quân; mỗi nước đi tốt/bắt quân đặt bộ đếm về 0 |
| Thiếu quân chắc chắn | Theo P-07: tự hòa với Vua–Vua, Vua+Tượng–Vua, Vua+Mã–Vua; hoặc mỗi bên chỉ Vua+Tượng và hai Tượng ở ô cùng màu |

Thứ tự xét sau một nước: xác định có nước hợp lệ hay không để phân biệt chiếu hết/hết nước; nếu ván còn có nước thì xét các quy tắc hòa tự động. Vì vậy nước tạo chiếu hết không bị đổi thành hòa do bộ đếm 50 nước.

“Vị trí không thể chiếu hết” có nghĩa không bên nào có thể chiếu hết bằng bất kỳ chuỗi nước hợp lệ nào, rộng hơn các mẫu thiếu quân nêu trên. Không tự kết luận hòa chỉ vì một bên không đủ quân chiếu hết hoặc vì có hai Mã. Theo đề xuất bản đầu, các thế bế tắc đặc biệt ngoài mẫu đã nêu chưa được nhận diện tự động; có thể tiếp tục tới điều kiện kết thúc khác. Đây là giới hạn cần duyệt của Engine, không tuyên bố bao quát toàn bộ luật cờ giải đấu.

Tự hòa ở lần lặp thứ 3 và mốc 50 nước là quy ước đơn giản hóa của ứng dụng; chưa có lệnh yêu cầu hòa hoặc xét nước dự định đi. Nếu muốn bám đầy đủ thủ tục luật giải đấu, phải sửa P-07, giao thức và kế hoạch trước khi lập trình. Không có tìm kiếm nước tốt, đánh giá thế cờ hoặc AI đối thủ.

## 8. Cấu trúc source code dự kiến

**Chỉ là minh họa trong tài liệu. Không tạo các đường dẫn này lúc khởi tạo tài liệu.** Tách vừa đủ theo trách nhiệm, chưa cần nhiều tầng thư mục.

```text
src/
  common/
    protocol.c / protocol.h       # đóng khung, kiểu thông điệp và mã lỗi
  server/
    main.c                        # khởi tạo và vòng đời server
    network.c / network.h         # select(), bộ đệm và gửi/nhận
    account.c / account.h         # tài khoản và phiên
    lobby.c / lobby.h             # sảnh và lời mời
    match.c / match.h             # điều phối trận
    storage.c / storage.h         # SQLite và xuất log
  chess/
    chess.c / chess.h             # trạng thái, nước đi và kết quả
  client/
    main.c                        # khởi tạo client
    cli.c / cli.h                 # nhập lệnh, nhận sự kiện và hiển thị
tests/                            # chỉ bổ sung trong giai đoạn triển khai
```

Network phía client có thể dùng vòng lặp `select()` theo dõi socket và đầu vào terminal để vẫn nhận sự kiện khi chờ người dùng nhập lệnh. Chia sẻ định dạng giao thức và quy ước tọa độ; không cần đưa toàn bộ Engine sang client. Tên file thực tế và cách build sẽ xác định ở giai đoạn triển khai, sau khi duyệt thiết kế.
