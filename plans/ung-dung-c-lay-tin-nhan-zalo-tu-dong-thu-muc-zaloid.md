# Ứng dụng C++ lấy tin nhắn Zalo tự động (thư mục zaloid)

## Bối cảnh

Workspace đang trống. Người dùng muốn một ứng dụng C++ tự động lấy tin/tin nhắn từ Zalo. Zalo không có API công khai cho tài khoản cá nhân, nên ứng dụng sẽ dùng Zalo Open API chính thức dành cho Official Account (OA) với access token do người dùng cung cấp. Ứng dụng poll định kỳ danh sách tin nhắn mới, loại trùng theo id tin nhắn, ghi ra console và file log.

## File sẽ đụng tới

- `zaloid/CMakeLists.txt`
- `zaloid/README.md`
- `zaloid/config.example.json`
- `zaloid/include/zaloid/config.hpp`
- `zaloid/include/zaloid/http_client.hpp`
- `zaloid/include/zaloid/zalo_api.hpp`
- `zaloid/include/zaloid/poller.hpp`
- `zaloid/src/main.cpp`
- `zaloid/src/config.cpp`
- `zaloid/src/http_client.cpp`
- `zaloid/src/zalo_api.cpp`
- `zaloid/src/poller.cpp`

## Ràng buộc không được phá

C++17, build bằng CMake. Chỉ dùng dependency phổ biến: libcurl (HTTP) và nlohmann/json (lấy qua FetchContent để không cần cài sẵn). Tuân thủ điều khoản Zalo: chỉ gọi Open API chính thức openapi.zalo.me, không reverse-engineer tài khoản cá nhân. Token không hardcode trong code — đọc từ file config hoặc biến môi trường.

## Các bước

1) Tạo cấu trúc thư mục zaloid/ (include, src). 2) Viết CMakeLists.txt: C++17, tìm libcurl, FetchContent nlohmann/json. 3) Viết HttpClient bọc libcurl (GET/POST kèm header access_token, xử lý lỗi mạng). 4) Viết Config: đọc token + chu kỳ poll từ config.json. 5) Viết ZaloApi: gọi endpoint /v3.0/oa/conversation/list và /v3.0/oa/message/list, parse JSON trả về model đơn giản (id, sender, nội dung, thời gian). 6) Viết Poller: vòng lặp mỗi N giây, lưu id tin nhắn đã thấy vào set để chống trùng, in tin mới ra console và ghi thêm vào zaloid.log. 7) main.cpp: khởi tạo, bắt Ctrl+C để thoát sạch. 8) Viết README hướng dẫn build, cách lấy access token OA trên developers.zalo.me, cách chạy.

## Cách kiểm chứng

Chạy `cmake -S zaloid -B zaloid/build && cmake --build zaloid/build` biên dịch thành công không lỗi/warning nghiêm trọng. Chạy binary với token giả → chương trình in thông báo lỗi API rõ ràng rồi tiếp tục/thoát đúng cách chứ không crash. Kiểm tra logic chống trùng bằng cách chạy poll 2 lần liên tiếp khi mock phản hồi cùng một tin → tin thứ hai bị bỏ qua.

## Phương án đã cân nhắc và loại bỏ

- Reverse-engineer API tài khoản cá nhân Zalo: loại vì vi phạm điều khoản, dễ bị khóa tài khoản, không ổn định. - Webhook server nhận push thay vì poll: loại vì cần IP công khai/HTTPS, phức tạp hơn nhu cầu chạy local. - Dùng Qt/network module: loại vì quá nặng so với một tool dòng lệnh nhỏ.
