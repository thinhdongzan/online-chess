# Cờ vua trực tuyến — Bài tập lớn IT4062

Ứng dụng cho phép hai người chơi cờ vua qua mạng, đồng thời hỗ trợ nhiều người kết nối và nhiều trận diễn ra song song. Server quản lý trạng thái chính, kiểm tra nước đi và quyết định kết quả; client cung cấp giao diện dòng lệnh cho người chơi.

**Trạng thái hiện tại:** đã có tài liệu thiết kế và khung source gồm các file `.c`/`.h` rỗng trong `src/common`, `src/server`, `src/chess` và `src/client`. Chưa có mã triển khai, cấu hình build, thư viện được cài đặt hoặc test. Các chức năng dưới đây là chức năng **dự kiến**, chưa được triển khai. Thời gian triển khai dự kiến là 6 tuần sau khi duyệt thiết kế.

## Thành viên nhóm

| STT | Họ và tên | MSSV | GitHub |
| --- | --- | --- | --- |
| 1 | Nguyễn Đức Mạnh | 20235775 | [ManhNguyen213](https://github.com/ManhNguyen213) |
| 2 | Đồng Văn Thịnh | 20235840 | [thinhdongzan](https://github.com/thinhdongzan) |

## Công nghệ dự kiến

| Thành phần | Công nghệ / lựa chọn |
| --- | --- |
| Ngôn ngữ | C cho client, server và Chess Engine |
| Kết nối | Mô hình Client–Server qua TCP Socket |
| Nhiều kết nối | Server dùng `select()` để theo dõi các socket |
| Dữ liệu | SQLite lưu tài khoản, điểm và lịch sử trận |
| Giao diện | Terminal CLI: giao diện dòng lệnh trên terminal |
| Luật cờ | Chess Engine tự lập trình bằng C, chạy trên server |

Socket là đầu mối giao tiếp qua mạng; TCP cung cấp luồng byte tin cậy, có thứ tự. `select()` cho phép server chờ sự kiện trên nhiều socket trong một vòng lặp. Chess Engine ở đây là bộ xử lý luật cờ, không phải AI chọn nước đi.

## Chức năng dự kiến

- Đăng ký, đăng nhập và quản lý phiên.
- Xem người chơi online chưa tham gia ván; gửi, chấp nhận hoặc từ chối lời thách đấu trong giới hạn xếp hạng.
- Chơi cờ, nhận nước đi đối phương; server kiểm tra luật và xác định kết quả.
- Tính điểm: thắng +3, hòa 0, thua -3.
- Lưu lịch sử và log gồm thời gian, IP hai bên, danh sách nước đi; gửi kết quả và file log khi kết thúc trận.
- Xin ngừng ván, chịu xử thua và có thể tham gia ván mới.

Phiên bản đầu chưa có GUI (giao diện đồ họa), AI đối thủ hoặc đồng hồ thi đấu. Những vấn đề đề bài chưa quy định được đánh dấu **Đề xuất thiết kế**, chưa coi là quyết định đã chốt.

## Tài liệu và thứ tự đọc

1. [Yêu cầu và phạm vi](docs/requirements.md): phân biệt yêu cầu gốc, quyết định đã chốt và đề xuất cần duyệt.
2. [Thiết kế kỹ thuật](docs/technical-design.md): kiến trúc, module, API dự kiến, dữ liệu, giao thức và luật cờ.
3. [Kế hoạch triển khai](docs/implementation-plan.md): công việc, phụ thuộc và cách kiểm tra trong 6 tuần.

Chưa có hướng dẫn build hoặc chạy vì chương trình chưa được triển khai.
