# Kế hoạch triển khai 6 tuần

Đây là kế hoạch công việc **tương lai**, chưa có phần mềm hoặc test được triển khai. Lần khởi tạo tài liệu này không tạo source, cấu hình build, thư viện hoặc test. Các mã FR/P tham chiếu [yêu cầu](requirements.md); API và giao thức tham chiếu [thiết kế kỹ thuật](technical-design.md).

Kế hoạch không giả định số thành viên. Mạng, Chess Engine và lưu trữ là ba nhánh có thể phát triển song song sau khi thống nhất dữ liệu/giao thức. Nếu nguồn lực hạn chế, thực hiện xen kẽ theo phụ thuộc; không cần chia sẵn một người cho mỗi module.

## Tuần 1 — Chốt thiết kế và dựng nền giao tiếp

**Mục tiêu:** thống nhất các đề xuất ảnh hưởng liên module, có client/server TCP tối thiểu để kiểm chứng cách truyền thông điệp.

**Công việc chính:**

1. Duyệt P-01 đến P-11, đặc biệt xếp hạng, mất kết nối, phạm vi hòa, môi trường POSIX và định dạng giao thức; cập nhật tài liệu theo quyết định.
2. Sau khi duyệt, mới tạo cấu trúc C tối thiểu, cấu hình build và chọn thư viện JSON, băm mật khẩu cùng cách dùng SQLite.
3. Làm server `select()` và client CLI tối thiểu; thực hiện bộ đệm, khung độ dài + JSON, đọc/ghi một phần và giới hạn khung.
4. Song song nếu phù hợp: chốt biểu diễn `GameState` và tạo bàn cờ xuất phát; phác dữ liệu rồi khởi tạo ba bảng SQLite theo thiết kế đã duyệt.

**Phần cần hoàn thành trước:** bộ tài liệu được xem xét; có quyết định về môi trường triển khai và các quy ước dữ liệu chung. Chưa cần Engine hoàn chỉnh để làm phần mạng.

**Kết quả chạy được / cách kiểm tra cuối tuần:** nhiều client kết nối; server nhận thông điệp JSON thử nghiệm. Kiểm tra một khung bị chia thành nhiều lần gửi, nhiều khung gộp chung, độ dài sai và một client chậm; client khác vẫn được phục vụ. Kiểm tra bàn cờ xuất phát và SQLite mở/đọc/ghi dữ liệu tối thiểu độc lập. Thông điệp thử nghiệm chỉ phục vụ kiểm tra truyền nhận, không mở thêm chức năng sản phẩm.

## Tuần 2 — Tài khoản, phiên và nước đi cơ bản

**Mục tiêu:** đăng ký/đăng nhập qua mạng; Chess Engine và lưu trữ tiến triển độc lập trước khi ghép trận.

**Công việc chính:**

1. Làm Account/Session: kiểm tra dữ liệu đăng ký, băm/xác thực mật khẩu, đăng nhập, đăng xuất và từ chối phiên trùng theo P-02/P-10 (FR-01/02).
2. Làm truy vấn người dùng, điểm và thứ hạng P-01; kiểm tra trùng tên và dùng truy vấn tham số.
3. Phát triển Engine: nước đi từng quân, cản đường, bắt quân, xác định ô bị tấn công và không để Vua mình bị chiếu.
4. Hoàn thiện CLI cho tài khoản và hiển thị phản hồi; xử lý EOF/lỗi socket, dọn phiên và giải phóng bộ đệm.

**Phần cần hoàn thành trước:** khung giao thức/vòng lặp mạng từ tuần 1; bảng `users` và quy ước mật khẩu đã chốt. Engine chỉ cần mô hình bàn cờ, không chờ phần tài khoản.

**Kết quả chạy được / cách kiểm tra cuối tuần:** đăng ký, đăng nhập, thoát và đăng nhập lại; thử sai mật khẩu, trùng tên, phiên trùng, lệnh chưa xác thực. Điểm và tài khoản vẫn tồn tại sau khởi động lại. Kiểm tra Engine bằng các thế nhỏ: quân bị chắn, đi sai hướng, tự chiếu Vua và thoát chiếu. Chưa cần chơi cả trận qua mạng.

## Tuần 3 — Sảnh, thách đấu và trận hai người tối thiểu

**Mục tiêu:** hai người vào được cùng một trận; nhiều cặp hoạt động độc lập.

**Công việc chính:**

1. Làm `LIST_READY`, phân trang và thông tin hạng/cờ lời mời; kiểm tra chênh lệch hạng đúng biên 10/11 (FR-04/05).
2. Làm tạo/chấp nhận/từ chối/hết hạn/hủy lời mời; kiểm tra lại điều kiện tại lúc chấp nhận và xử lý lời mời chéo (FR-06).
3. Tạo Match, gán màu theo P-04, khóa hai người khỏi trận khác, gửi `MATCH_STARTED` và hiển thị bàn cờ CLI.
4. Ghép `MOVE` với Engine cơ bản, kiểm tra người thuộc trận/lượt/`expected_ply`, ghi nước hợp lệ rồi gửi `MOVE_APPLIED` (FR-07 và một phần FR-08).
5. Song song hoàn thiện cách ghi trận/nước đi có giao dịch; lưu thời gian bắt đầu và IP hai bên ngay khi tạo trận (một phần FR-09).

**Phần cần hoàn thành trước:** phiên/tài khoản từ tuần 2, xếp hạng đã chốt; API Engine cơ bản và API lưu nước đi đã dùng được. Nhánh Lobby/Challenge có thể làm trong khi nhánh Engine bổ sung luật đặc biệt.

**Kết quả chạy được / cách kiểm tra cuối tuần:** ít nhất hai trận với bốn client, nước đi không lẫn sang trận khác. Kiểm tra người offline/đang chơi không thuộc danh sách sẵn sàng, người có lời mời vẫn có cờ đúng. Lời mời chéo/chấp nhận muộn/đổi hạng không tạo hai trận cho cùng người. Nước sai không đổi bàn cờ hoặc SQLite; ghi rõ Engine ở mốc này chưa đủ toàn bộ luật.

## Tuần 4 — Luật cờ và quyết toán kết quả

**Mục tiêu:** hoàn thành phạm vi Engine đã duyệt và kết thúc trận với điểm nhất quán.

**Công việc chính:**

1. Hoàn thiện nhập thành, bắt tốt qua đường, phong cấp và ảnh hưởng của chúng tới an toàn Vua.
2. Làm chiếu hết, hết nước hợp lệ, lặp vị trí, bộ đếm 50 nước và các mẫu thiếu quân theo P-07 đã duyệt (FR-08).
3. Hoàn thiện `match_finish` và giao dịch ghi nước cuối/kết quả/điểm; đảm bảo mỗi trận chỉ cộng/trừ một lần (FR-03).
4. Làm `RESIGN`, xử thua khi phát hiện mất kết nối và trả phiên còn online về sảnh; xử lý lỗi ghi kết quả và phục hồi `ABORTED` khi server chạy lại (FR-11; P-05/P-06).
5. Ghép `MATCH_RESULT` với CLI, cập nhật điểm và danh sách sẵn sàng sau trận.

**Phần cần hoàn thành trước:** Match và lưu nước đi từ tuần 3; lịch sử vị trí của Engine phải đủ dữ liệu cho lặp vị trí. Quyết toán cần schema và giao dịch SQLite, nhưng không cần đợi xuất file log.

**Kết quả chạy được / cách kiểm tra cuối tuần:** chơi tới thắng/hòa, hoặc đầu hàng rồi tham gia trận mới; kiểm tra điểm +3/0/-3, kể cả điểm âm. Kiểm tra lặp yêu cầu kết thúc không cộng điểm lại, mất kết nối không làm hỏng trận khác và sự cố lưu trữ không công bố kết quả chưa được lưu. Dùng thế cờ ngắn để kiểm tra từng luật đặc biệt và chiếu hết tại mốc bộ đếm hòa; không phải chơi thủ công đủ 100 nửa nước cho mọi lần kiểm tra.

## Tuần 5 — Lịch sử, file log và ghép toàn bộ luồng

**Mục tiêu:** hoàn thành yêu cầu lưu/nhận log, hỗ trợ tải lại và luồng sử dụng xuyên suốt.

**Công việc chính:**

1. Hoàn thiện log UTF-8 từ SQLite: thời gian bắt đầu/kết thúc, IP hai người, toàn bộ nước hợp lệ, kết quả và nguyên nhân (FR-09).
2. Tự gửi `LOG_BEGIN`/`LOG_CHUNK`/`LOG_END` sau kết quả cho hai client; ghi file tạm và xác nhận đủ byte/khối ở client (FR-10).
3. Làm `HISTORY` phân trang và `GET_LOG`, kiểm tra quyền người tham gia và tải lại khi đứt kết nối (P-08).
4. Ghép đầy đủ CLI: nhận lời mời/nước đi/kết quả khi đang chờ nhập, thông báo lỗi dễ hiểu, trở lại sảnh và mở trận mới ngay sau kết thúc.
5. Kiểm tra truyền log xen sự kiện, giới hạn hàng đợi, client chậm và khôi phục sau khởi động lại theo P-06.

**Phần cần hoàn thành trước:** kết quả/điểm đã quyết toán đúng từ tuần 4, nước đi được lưu đầy đủ; cơ chế gửi theo khối và bộ đệm mạng từ tuần 1. Phần trình bày log có thể chuẩn bị song song với luật cờ, nhưng chỉ xác nhận toàn luồng khi đã có kết quả bền vững.

**Kết quả chạy được / cách kiểm tra cuối tuần:** hai người nhận cùng kết quả và file log hoàn chỉnh; đối chiếu nội dung với SQLite. Ngắt mạng khi tải, đăng nhập lại, xem lịch sử và tải lại được; người ngoài trận bị từ chối. Tải log không giữ người chơi ở trạng thái trong trận và không chặn các trận khác.

## Tuần 6 — Rà soát yêu cầu, sửa lỗi và chuẩn bị demo

**Mục tiêu:** có phiên bản ổn định trong phạm vi đã duyệt, kiểm chứng đủ FR-01 đến FR-11.

**Công việc chính:**

1. Đối chiếu từng FR với kịch bản chạy; bổ sung kiểm tra tích hợp còn thiếu, tập trung ranh giới thách đấu, luật đặc biệt và kết thúc trận.
2. Chạy nhiều client/nhiều trận, thử tách/gộp khung TCP, dữ liệu sai, client chậm, ngắt kết nối, SQLite lỗi và server khởi động lại.
3. Kiểm tra tính nhất quán bàn cờ–lịch sử–điểm–log, tài nguyên socket/bộ nhớ và giới hạn `select()`; ghi môi trường/số kết nối đã thử, không tự suy ra khả năng tải cao hơn.
4. Sửa lỗi và cập nhật tài liệu theo đúng chức năng thực tế; lúc đó mới bổ sung hướng dẫn build/chạy tương ứng với chương trình đã có.
5. Chuẩn bị dữ liệu và kịch bản demo: tài khoản, thách đấu hợp lệ/bị chặn, hai trận song song, nước sai, kết thúc, nhận log và vào ván mới.

**Phần cần hoàn thành trước:** luồng tích hợp đầy đủ của tuần 5; các điểm cần duyệt đã được quyết định và ghi lại. Không mở rộng GUI/AI/đồng hồ trong tuần ổn định này.

**Kết quả chạy được / cách kiểm tra cuối tuần:** demo được toàn bộ yêu cầu gốc trong phạm vi đã thống nhất; ghi nhận kết quả kiểm tra và giới hạn còn lại. Tài liệu phân biệt rõ phần đã làm và phần chưa làm; chưa coi đề xuất chưa duyệt là chức năng đã cam kết.

## Quan hệ giữa ba nhánh công việc

| Giai đoạn | Network / CLI | Chess Engine | Storage/Logging | Điểm ghép |
| --- | --- | --- | --- | --- |
| Tuần 1–2 | Khung TCP, `select()`, tài khoản | Trạng thái và luật cơ bản | Tài khoản, hạng, dữ liệu trận | Thống nhất kiểu dữ liệu, mã lỗi và cách trả kết quả |
| Tuần 3–4 | Sảnh, lời mời, nước đi, kết quả | Luật đặc biệt và điều kiện kết thúc | Ghi nước, quyết toán một lần, phục hồi sự cố | Match gọi Engine, ghi thành công rồi mới phát sự kiện |
| Tuần 5–6 | Nhận log, hoàn thiện luồng, kiểm tra nhiều client | Kiểm chứng các thế biên, sửa lỗi | Xuất log, lịch sử, quyền truy cập | Đối chiếu kết quả–điểm–log qua toàn bộ luồng |

Song song là khả năng tổ chức công việc, không phải yêu cầu có nhiều thành viên. Không cần chờ Engine hoàn chỉnh mới làm mạng hoặc SQLite; không công bố một nước hay kết quả thành công trước khi lưu dữ liệu tương ứng.

## Bước lập trình đầu tiên sau khi duyệt thiết kế

Tạo một client C và một server C tối thiểu trên môi trường đã chọn: server dùng `select()` với socket không chặn, cả hai dùng khung 4 byte độ dài + JSON theo thiết kế đã duyệt. Kiểm chứng nhận đủ khung khi dữ liệu bị chia nhỏ, tách nhiều khung gộp chung và phục vụ được hai client độc lập. Đây là nền để thêm đăng nhập và thách đấu; chưa ghép luật cờ ở bước đầu.

Chỉ thực hiện bước này sau khi duyệt thiết kế, không thuộc phạm vi khởi tạo bốn tài liệu hiện tại.
