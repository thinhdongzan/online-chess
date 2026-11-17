# Yêu cầu và phạm vi

Tài liệu này là mốc tham chiếu cho [thiết kế kỹ thuật](technical-design.md) và [kế hoạch 6 tuần](implementation-plan.md). Hiện tại chỉ có tài liệu; mọi hành vi phần mềm mô tả dưới đây đều là dự kiến.

## 1. Mục tiêu và phạm vi

Xây dựng ứng dụng cờ vua trực tuyến giữa hai người chơi bằng C, theo mô hình Client–Server qua TCP Socket. Nhiều người có thể kết nối và nhiều trận có thể diễn ra đồng thời. Server là nguồn trạng thái chính: client gửi yêu cầu, server kiểm tra rồi mới cập nhật và thông báo.

Thời gian triển khai: 6 tuần. Phạm vi bao gồm tài khoản, phiên, sảnh người chơi, thách đấu, chơi cờ, tính điểm, lịch sử và log trận. Sảnh (Lobby) là nơi xem người chơi và gửi lời mời trước khi vào trận.

## 2. Yêu cầu chức năng của đề bài

Các mã `FR` (Functional Requirement — yêu cầu chức năng) dùng để đối chiếu thiết kế và kế hoạch; không biểu thị chức năng đã hoàn thành.

| Mã | Yêu cầu gốc | Cách kiểm tra dự kiến |
| --- | --- | --- |
| FR-01 | Đăng ký tài khoản | Tạo tài khoản mới; tài khoản có thể dùng để đăng nhập |
| FR-02 | Đăng nhập và quản lý phiên | Server nhận diện người đã đăng nhập; yêu cầu cần đăng nhập không được thực hiện bởi khách chưa đăng nhập |
| FR-03 | Thắng +3, hòa 0, thua -3 | Đối chiếu điểm hai bên trước/sau từng loại kết quả |
| FR-04 | Liệt kê người chơi sẵn sàng: online và chưa tham gia ván | Người offline hoặc đang trong trận không xuất hiện trong danh sách sẵn sàng |
| FR-05 | Gửi thách đấu người cách mình tối đa 10 bậc xếp hạng | Chênh lệch 10 được phép; chênh lệch 11 bị từ chối |
| FR-06 | Chấp nhận hoặc từ chối lời thách đấu | Người nhận phản hồi; chấp nhận hợp lệ tạo một trận cho hai bên |
| FR-07 | Truyền nước đi giữa hai người chơi | Nước đi được server chấp nhận được thông báo cho cả hai bên |
| FR-08 | Server kiểm tra nước đi hợp lệ và xác định kết quả | Sai lượt/sai luật bị từ chối; client không tự quyết định kết quả |
| FR-09 | Ghi log thời gian bắt đầu/kết thúc, IP hai người và danh sách nước đi | Đối chiếu log với một trận đã chơi |
| FR-10 | Kết thúc trận, gửi kết quả và file log cho hai người chơi | Hai client nhận cùng kết quả và file log đầy đủ |
| FR-11 | Cho phép xin ngừng ván; người xin ngừng bị xử thua và có thể tham gia ván mới | Trận kết thúc với bên xin ngừng thua; phiên còn hoạt động trở lại sảnh |

Trong tài liệu, “xin ngừng ván” dùng lệnh `RESIGN` (đầu hàng), không có bước chờ đối phương đồng ý.

## 3. Điểm và giới hạn thách đấu

| Quy tắc | Phân loại | Nội dung |
| --- | --- | --- |
| Điểm sau trận | Yêu cầu gốc | Thắng +3, hòa 0, thua -3 |
| Khoảng cách thách đấu | Yêu cầu gốc | Hai bậc xếp hạng cách nhau tối đa 10, tính cả biên: `abs(rank_a - rank_b) <= 10` |
| Điểm khởi tạo và điểm âm | Đề xuất thiết kế P-01 | Tài khoản mới có 0 điểm; cho phép điểm âm để luôn trừ đúng 3 khi thua |
| Cách xếp hạng | Đề xuất thiết kế P-01 | Xếp toàn bộ tài khoản, kể cả offline, theo điểm giảm dần; bằng điểm thì `user_id` tăng dần. Mỗi người có một vị trí riêng, bắt đầu từ 1 |
| Thời điểm xét giới hạn | Đề xuất thiết kế P-03 | Kiểm tra khi gửi và khi chấp nhận lời mời theo bảng xếp hạng mới nhất; nếu không còn đạt điều kiện thì hủy lời mời |

Ví dụ theo P-01: người hạng 7 có thể thách đấu hạng 17, nhưng không thể thách đấu hạng 18. “Bậc” là vị trí xếp hạng, không phải chênh lệch điểm. Khi bằng điểm, thứ tự theo `user_id` là quy tắc đơn giản, ổn định, nhưng có thể làm người mới cùng 0 điểm ở xa nhau; cần duyệt lựa chọn này.

## 4. Những quyết định đã chốt

Đây là ràng buộc từ thông tin dự án và yêu cầu người dùng, không phải đề xuất bổ sung:

- Dùng C; kiến trúc Client–Server; giao tiếp bằng TCP Socket.
- Server xử lý nhiều kết nối bằng `select()`; hỗ trợ nhiều trận đồng thời.
- SQLite lưu tài khoản, điểm và lịch sử trận.
- Client dùng Terminal CLI; Chess Engine tự lập trình bằng C.
- Server quản lý trạng thái chính, kiểm tra nước đi và quyết định kết quả.
- Triển khai trong 6 tuần; chưa làm GUI, AI đối thủ hoặc đồng hồ thi đấu.
- Lần khởi tạo này chỉ tạo bốn tài liệu Markdown; chưa tạo source, cấu hình build, cài thư viện hoặc tạo test.

## 5. Đề xuất bổ sung và điểm cần thống nhất

**Toàn bộ các dòng P dưới đây là Đề xuất thiết kế, chưa phải yêu cầu gốc hoặc quyết định đã duyệt.** Thiết kế kỹ thuật và kế hoạch dùng chúng làm phương án tham chiếu để có thể lập trình sau khi thống nhất.

| Mã | Đề xuất thiết kế | Điểm cần duyệt / hệ quả |
| --- | --- | --- |
| P-01 | Điểm ban đầu 0, cho phép âm; xếp hạng toàn bộ tài khoản theo điểm giảm dần rồi `user_id` tăng dần | Thống nhất cách xử lý bằng điểm và tập tài khoản được xếp hạng |
| P-02 | Một tài khoản có một phiên online và tối đa một trận; đăng nhập trùng bị từ chối. Phiên gắn với kết nối TCP, chưa có khôi phục phiên | Phải đăng nhập lại khi kết nối mới; server lấy danh tính từ phiên, không tin `user_id` do client tự khai |
| P-03 | Mỗi người chỉ liên quan tối đa một lời mời đang chờ; hết hạn sau 60 giây. Kiểm tra lại điều kiện khi chấp nhận; logout/mất kết nối/hết hạn hủy lời mời | Người đang có lời mời vẫn online và chưa vào trận, nên vẫn có trong danh sách sẵn sàng, kèm cờ có lời mời; lời mời mới bị từ chối với `BUSY` |
| P-04 | Người gửi thách đấu cầm Trắng; Trắng đi trước | Đơn giản cho bản đầu, chưa cân bằng màu giữa nhiều trận |
| P-05 | Mất kết nối được server phát hiện trong trận thì bên đó thua ngay; không nối lại trận. Ngoài trận thì hủy phiên và lời mời | Chỉ áp dụng khi thấy đầu bên kia đóng luồng (EOF) hoặc lỗi socket; mạng im lặng không đồng nghĩa đã mất kết nối. Chưa có heartbeat (thông điệp định kỳ kiểm tra kết nối) hay giới hạn thời gian đi |
| P-06 | Trận đang chơi khi server khởi động lại được đánh dấu `ABORTED`, không cộng/trừ điểm; lịch sử và nước đi đã ghi được giữ lại | Đây là ngoại lệ sự cố, không phải hòa; phiên và lời mời cũ không khôi phục |
| P-07 | Xử lý đầy đủ nước đi thường, chiếu, nhập thành, bắt tốt qua đường, phong cấp, chiếu hết, hết nước hợp lệ. Tự hòa khi lặp vị trí 3 lần hoặc đủ 50 nước mỗi bên không đi tốt/bắt quân; nhận diện một số thế thiếu quân chắc chắn không thể chiếu hết | Quy tắc tự hòa 3 lần/50 nước là lựa chọn của ứng dụng, không tuyên bố tương thích đầy đủ luật giải đấu. Phạm vi thế hòa chi tiết ở thiết kế kỹ thuật cần duyệt |
| P-08 | SQLite lưu dữ liệu log; server dựng file văn bản UTF-8 và tự gửi cho hai client sau trận. Cho tải lại từ lịch sử nếu truyền lỗi hoặc đang offline | Chỉ hai người tham gia được tải. Sau mất kết nối, bên offline nhận lại kết quả/log khi đăng nhập và mở lịch sử |
| P-09 | Chọn môi trường POSIX (chuẩn giao diện lập trình phổ biến trên hệ Unix), ưu tiên Linux; socket không chặn, có bộ đệm, giới hạn thông điệp và hàng đợi gửi | Cần thống nhất hệ điều hành dùng khi chấm/demo. Số kết nối chịu giới hạn `select()` và tài nguyên, chưa đặt chỉ tiêu tải cụ thể |
| P-10 | Mật khẩu được băm (biến đổi một chiều để kiểm tra khi đăng nhập) bằng thuật toán chuyên dụng có salt qua thư viện phù hợp, không lưu rõ và không tự viết hàm băm. Bản đầu demo trên mạng tin cậy, chưa có TLS | Salt là giá trị ngẫu nhiên riêng cho mỗi mật khẩu. TLS là lớp mã hóa đường truyền; TCP thuần không bảo vệ mật khẩu khi truyền. Chọn thư viện ở giai đoạn triển khai, chưa cài lúc này |
| P-11 | Dùng giao thức khung độ dài + JSON UTF-8; SQLite ghi dữ liệu trận và điểm theo giao dịch; mỗi trận chỉ quyết toán một lần | Chốt định dạng, giới hạn và cách xử lý lỗi trước khi viết phần mạng/lưu trữ; JSON là định dạng dữ liệu văn bản có cấu trúc |

Nếu cả hai bên mất kết nối, P-05 lấy sự kiện mà server xử lý trước làm nguyên nhân kết thúc; sự kiện sau không thay đổi kết quả hoặc tính điểm lần nữa. Cách này dễ triển khai nhưng phụ thuộc thứ tự phát hiện, cũng cần được xem xét khi duyệt P-05.

## 6. Những tính năng không làm trong phiên bản đầu

| Nhóm | Phạm vi loại trừ | Cơ sở |
| --- | --- | --- |
| Giao diện / đối thủ / thời gian | GUI, AI đối thủ, đồng hồ thi đấu | Đã chốt trong thông tin dự án |
| Tiện ích chơi | Xem trận, chat, giải đấu, ghép cặp tự động, xin đi lại, tạm dừng ván | Đề xuất thiết kế: ngoài phạm vi bản đầu |
| Kết nối nâng cao | Nối lại trận, nhiều thiết bị cho cùng tài khoản, heartbeat ở tầng ứng dụng | Đề xuất thiết kế: ngoài phạm vi bản đầu theo P-02/P-05 |
| Luật giải đấu đầy đủ | Khiếu nại hòa cho vị trí sắp xuất hiện, xử lý mọi thế cờ không thể chiếu hết, thủ tục trọng tài và hòa theo thỏa thuận | Đề xuất thiết kế: dùng phạm vi Engine tại P-07; cần duyệt trước triển khai |
| Vận hành công khai | TLS, triển khai Internet công khai, hệ thống chống gian lận hoặc chống lạm dụng hoàn chỉnh | Đề xuất thiết kế: bản đầu phục vụ học tập/demo theo P-10 |

Việc loại trừ các chức năng bổ sung không làm thay đổi các yêu cầu gốc FR-01 đến FR-11.
