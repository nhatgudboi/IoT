#IoT
HỆ THỐNG NHÀ THÔNG MINH (IoT)
Giới thiệu
Hệ thống nhà thông minh (Smart Home System) là ứng dụng IoT giúp người dùng điều khiển và giám sát các thiết bị điện trong nhà (như đèn, quạt, máy lạnh, cửa, cảm biến môi trường, v.v.) từ xa thông qua smartphone hoặc web.
 Hệ thống hỗ trợ tự động hóa theo kịch bản, giám sát trạng thái thiết bị theo thời gian thực và lưu trữ dữ liệu trên Firebase.
Chức năng chính
1. Điều khiển thiết bị điện từ xa
Bật/tắt đèn, quạt, máy lạnh, hoặc các thiết bị khác qua:


Ứng dụng di động (Android/iOS)


Giao diện web điều khiển


Điều khiển theo phòng hoặc thiết bị riêng lẻ


Cập nhật trạng thái thiết bị theo thời gian thực


2. Giám sát cảm biến thời gian thực
Cảm biến nhiệt độ, độ ẩm, ánh sáng, chuyển động, khí gas, v.v.


Dữ liệu được hiển thị và cập nhật liên tục trên giao diện người dùng


Cảnh báo tự động khi có thông số vượt ngưỡng




3. Tự động hóa (Automation)
Thiết lập kịch bản tự động:


Ví dụ: Bật quạt khi nhiệt độ > 30°C


Tắt đèn khi không có chuyển động sau 5 phút


Người dùng có thể bật/tắt các kịch bản này từ giao diện web hoặc app


4. Quản lý người dùng
Đăng ký / Đăng nhập (qua Firebase Authentication)


Phân quyền người dùng: chủ nhà, thành viên, khách


Lưu lịch sử điều khiển và hoạt động của thiết bị
Công nghệ sử dụng

Thành phần
Công nghệ / Thiết bị
Vi điều khiển IoT
ESP32
Ngôn ngữ lập trình IoT
Arduino (C/C++)
Cơ sở dữ liệu
Firebase Realtime Database
Xác thực người dùng
Firebase Authentication
Giao diện Web
HTML, CSS, JavaScript
Giao diện App
Android Studio
Giao tiếp IoT
MQTT


Cách hoạt động tổng quát
ESP32/NodeMCU thu thập dữ liệu cảm biến và gửi lên Firebase.


Giao diện Web lắng nghe thay đổi dữ liệu trong Firebase Realtime Database → hiển thị trạng thái.


Khi người dùng bật/tắt thiết bị → Firebase cập nhật → ESP32 đọc dữ liệu mới và thực thi lệnh điều khiển.


Automation logic tự động kích hoạt khi điều kiện được thỏa mãn.


Môi trường phát triển
Arduino IDE


Firebase Console


Visual Studio Code (cho web)
