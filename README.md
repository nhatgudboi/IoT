# 🏠 SmartHome IoT Dashboard - ESP32

Hệ thống tự động hóa nhà thông minh sử dụng ESP32, tích hợp cảm biến khí gas, cảm biến ánh sáng, điều khiển đèn thông minh, và nhận diện khuôn mặt AI để mở cửa tự động.

## 📋 Mục lục

- [Tổng quan](#tổng-quan)
- [Tính năng](#tính-năng)
- [Cấu trúc dự án](#cấu-trúc-dự-án)
- [Yêu cầu hệ thống](#yêu-cầu-hệ-thống)
- [Cài đặt](#cài-đặt)
- [Cấu hình](#cấu-hình)
- [Sử dụng](#sử-dụng)
- [Công nghệ sử dụng](#công-nghệ-sử-dụng)
- [Sơ đồ kết nối phần cứng](#sơ-đồ-kết-nối-phần-cứng)
- [Troubleshooting](#troubleshooting)

## 🎯 Tổng quan

Dự án SmartHome IoT là một hệ thống tự động hóa nhà thông minh hoàn chỉnh, bao gồm:

- **ESP32 Microcontroller**: Xử lý cảm biến và điều khiển thiết bị
- **Web Dashboard**: Giao diện web real-time để theo dõi và điều khiển
- **Firebase Realtime Database**: Đồng bộ dữ liệu giữa ESP32 và web
- **AI Face Recognition**: Nhận diện khuôn mặt để mở cửa tự động (Python + Web)

## ✨ Tính năng

### 🔥 An toàn & Báo động
- **Cảm biến khí gas**: Phát hiện khí gas độc hại với ngưỡng cảnh báo
- **Tự động mở cửa sổ**: Khi phát hiện gas, hệ thống tự động mở cửa sổ và cửa chính để thoát khí
- **Còi báo động**: Cảnh báo âm thanh khi phát hiện gas
- **Đèn LED báo động**: Nhấp nháy khi có nguy hiểm

### 💡 Đèn thông minh
- **Cảm biến ánh sáng**: Tự động phát hiện độ sáng môi trường
- **Chế độ tự động**: Tự động bật/tắt đèn dựa trên cảm biến ánh sáng
- **Chế độ thủ công**: Điều khiển đèn từ web dashboard
- **Đồng bộ real-time**: Trạng thái đèn được cập nhật ngay lập tức

### 🤖 AI Face Recognition
- **Nhận diện khuôn mặt (Python)**: Sử dụng webcam để nhận diện và mở cửa tự động
- **Nhận diện khuôn mặt (Web)**: Sử dụng camera trình duyệt để nhận diện trực tiếp trên web
- **Độ chính xác cao**: Yêu cầu 8 frame liên tiếp nhận diện đúng mới mở cửa
- **Train nhiều ảnh**: Hỗ trợ train nhiều ảnh để tăng độ chính xác
- **Tự động mở/đóng cửa**: Mở cửa 5 giây sau khi nhận diện thành công

### 📊 Web Dashboard
- **Real-time monitoring**: Theo dõi trạng thái thiết bị theo thời gian thực
- **Giao diện hiện đại**: Thiết kế glassmorphism, responsive, đẹp mắt
- **Nhật ký hoạt động**: Ghi lại tất cả các sự kiện quan trọng
- **Trạng thái kết nối**: Hiển thị trạng thái kết nối với ESP32

## 📁 Cấu trúc dự án

```
WebIoT/
├── index.html              # Giao diện web dashboard
├── styles.css              # Styling cho web dashboard
├── app.js                  # Logic JavaScript cho web dashboard
├── esp32_smarthome.ino     # Code Arduino cho ESP32
├── nhandienkhuonmat.py     # Script Python nhận diện khuôn mặt
├── smarthome-iot-2d485-firebase-adminsdk-fbsvc-2562115f7a.json  # Firebase Admin SDK key
└── README.md               # Tài liệu dự án
```

## 🔧 Yêu cầu hệ thống

### Phần cứng
- **ESP32 Development Board** (ESP32-WROOM-32 hoặc tương đương)
- **Cảm biến khí gas MQ-2** (kết nối với GPIO 34)
- **Cảm biến ánh sáng** (Digital Output, kết nối với GPIO 35)
- **Servo Motor** x3 (Cửa chính GPIO 13, Cửa sổ 1 GPIO 25, Cửa sổ 2 GPIO 26)
- **Buzzer** (GPIO 12)
- **LED** x2 (Đèn báo động GPIO 14, Đèn thông minh GPIO 27)
- **Webcam** (cho Python face recognition)
- **Dây nối và breadboard**

### Phần mềm
- **Arduino IDE** (v1.8.x hoặc v2.x)
- **Python** 3.7+
- **Trình duyệt web** hiện đại (Chrome, Firefox, Edge)
- **Firebase Account** (miễn phí)

### Thư viện Arduino
- `WiFi.h` (built-in)
- `FirebaseESP32` (cài từ Library Manager)
- `ESP32Servo` (cài từ Library Manager)

### Thư viện Python
- `opencv-python`
- `face-recognition`
- `firebase-admin`
- `numpy`

## 🚀 Cài đặt

### 1. Cài đặt ESP32

#### Bước 1: Cài đặt Arduino IDE và ESP32 Board
1. Tải và cài đặt [Arduino IDE](https://www.arduino.cc/en/software)
2. Mở Arduino IDE → File → Preferences
3. Thêm URL vào "Additional Board Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Tools → Board → Boards Manager → Tìm "ESP32" → Install

#### Bước 2: Cài đặt thư viện
1. Sketch → Include Library → Manage Libraries
2. Tìm và cài đặt:
   - `Firebase ESP32 Client` (bởi Mobizt)
   - `ESP32Servo` (bởi Kevin Harrington)

#### Bước 3: Cấu hình code ESP32
1. Mở file `esp32_smarthome.ino`
2. Cập nhật thông tin WiFi:
   ```cpp
   const char* ssid = "TEN_WIFI_CUA_BAN";
   const char* password = "MAT_KHAU_WIFI";
   ```
3. Cập nhật Firebase credentials (đã có sẵn trong code)

#### Bước 4: Nạp code vào ESP32
1. Kết nối ESP32 với máy tính qua USB
2. Tools → Board → Chọn "ESP32 Dev Module"
3. Tools → Port → Chọn cổng COM của ESP32
4. Click "Upload" (mũi tên bên phải)

### 2. Cài đặt Python Face Recognition

#### Bước 1: Cài đặt Python
1. Tải và cài đặt [Python 3.7+](https://www.python.org/downloads/)
2. Đảm bảo chọn "Add Python to PATH" khi cài đặt

#### Bước 2: Cài đặt thư viện
Mở Terminal/Command Prompt và chạy:
```bash
pip install opencv-python face-recognition firebase-admin numpy
```

**Lưu ý**: `face-recognition` có thể cần cài đặt thêm dependencies trên Windows:
- Tải [Visual C++ Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/)
- Hoặc cài đặt từ [pre-built wheels](https://github.com/ageitgey/face_recognition/issues/175)

#### Bước 3: Cấu hình Firebase
1. Đổi tên file `smarthome-iot-2d485-firebase-adminsdk-fbsvc-2562115f7a.json` thành `serviceAccountKey.json`
2. Đặt file vào cùng thư mục với `nhandienkhuonmat.py`
3. Tạo thư mục `images/` và đặt ảnh khuôn mặt vào (ví dụ: `admin.jpg`, `admin_1.jpg`, `admin_2.jpg`)

#### Bước 4: Chạy script
```bash
python nhandienkhuonmat.py
```

### 3. Cài đặt Web Dashboard

#### Bước 1: Mở web dashboard
1. Mở file `index.html` bằng trình duyệt web
2. Hoặc sử dụng local server:
   ```bash
   # Python
   python -m http.server 8000
   
   # Node.js
   npx http-server
   ```
3. Truy cập: `http://localhost:8000`

#### Bước 2: Cấu hình Firebase (đã có sẵn)
- Firebase config đã được cấu hình trong `index.html`
- Không cần thay đổi gì nếu sử dụng cùng Firebase project

## ⚙️ Cấu hình

### Cấu hình ESP32

#### Ngưỡng cảm biến gas
```cpp
const int GAS_HIGH_THRESHOLD = 1000;  // Ngưỡng báo động
const int GAS_LOW_THRESHOLD = 900;    // Ngưỡng tắt báo động (hysteresis)
```

#### Chân GPIO
```cpp
const int gasPin = 34;           // Cảm biến khí gas
const int lightSensorPin = 35;   // Cảm biến ánh sáng
const int mainDoorPin = 13;      // Cửa chính (Servo)
const int window1Pin = 25;       // Cửa sổ 1 (Servo)
const int window2Pin = 26;       // Cửa sổ 2 (Servo)
const int buzzPin = 12;          // Còi báo động
const int alertLedPin = 14;      // Đèn báo động
const int smartLedPin = 27;      // Đèn thông minh
```

### Cấu hình Python Face Recognition

#### Tham số độ chính xác
```python
FACE_TOLERANCE = 0.35                    # Độ nghiêm ngặt (càng thấp càng chính xác)
FACE_DISTANCE_THRESHOLD = 0.35            # Ngưỡng khoảng cách
REQUIRED_CONSECUTIVE_MATCHES = 8          # Số frame liên tiếp cần nhận diện đúng
```

#### Đường dẫn ảnh
```python
IMAGE_PATH = "images/admin.jpg"           # Ảnh khuôn mặt chủ nhà
KEY_PATH = "serviceAccountKey.json"      # Firebase Admin SDK key
```

### Cấu hình Web Dashboard

#### Tham số nhận diện khuôn mặt (Web)
```javascript
const FACE_TOLERANCE = 0.35;
const FACE_DISTANCE_THRESHOLD = 0.35;
const REQUIRED_CONSECUTIVE_MATCHES = 8;
```

#### Timeout kết nối
```javascript
const DATA_TIMEOUT = 3000;  // 3 giây không có dữ liệu = mất kết nối
```

## 📖 Sử dụng

### Web Dashboard

1. **Theo dõi cảm biến khí gas**
   - Giá trị gas hiển thị real-time
   - Màu sắc thay đổi theo mức độ nguy hiểm:
     - Xanh lá: An toàn (< 300)
     - Vàng: Cảnh báo (300-1000)
     - Đỏ: Nguy hiểm (> 1000)

2. **Điều khiển đèn thông minh**
   - Toggle "Tự động" để bật chế độ tự động
   - Hoặc dùng nút "Bật/Tắt" ở chế độ thủ công

3. **AI Face Unlock (Web)**
   - Click "Bật Camera" để bật webcam
   - Click "Train Khuôn Mặt" và chọn ảnh để train
   - Hệ thống tự động nhận diện và mở cửa khi đủ 8 frame liên tiếp

4. **Xem nhật ký**
   - Tất cả sự kiện được ghi lại trong phần "Nhật ký hoạt động"
   - Click "Xóa nhật ký" để xóa log

### Python Face Recognition

1. **Chuẩn bị ảnh**
   - Đặt ảnh khuôn mặt vào thư mục `images/`
   - Tên file: `admin.jpg`, `admin_1.jpg`, `admin_2.jpg`, ...
   - Hỗ trợ nhiều ảnh để tăng độ chính xác

2. **Chạy script**
   ```bash
   python nhandienkhuonmat.py
   ```

3. **Nhận diện**
   - Webcam sẽ tự động bật
   - Khi nhận diện đúng 8 frame liên tiếp → Tự động mở cửa
   - Cửa sẽ tự động đóng sau 5 giây

## 🛠️ Công nghệ sử dụng

### Frontend
- **HTML5**: Cấu trúc trang web
- **CSS3**: Styling với glassmorphism, animations, responsive design
- **JavaScript (ES6+)**: Logic xử lý, Firebase integration
- **Face-api.js**: Nhận diện khuôn mặt trên trình duyệt

### Backend
- **Arduino C++**: Code cho ESP32
- **Python 3**: Script nhận diện khuôn mặt

### Cloud & Database
- **Firebase Realtime Database**: Đồng bộ dữ liệu real-time
- **Firebase Admin SDK**: Xác thực từ Python script

### Hardware
- **ESP32**: Microcontroller chính
- **MQ-2 Gas Sensor**: Cảm biến khí gas
- **Light Sensor**: Cảm biến ánh sáng
- **Servo Motors**: Điều khiển cửa/cửa sổ
- **Buzzer**: Còi báo động
- **LEDs**: Đèn báo động và đèn thông minh

## 🔌 Sơ đồ kết nối phần cứng

```
ESP32 Pinout:
┌─────────────────────────────────┐
│  ESP32                          │
│                                 │
│  GPIO 34 ←── MQ-2 Gas Sensor    │
│  GPIO 35 ←── Light Sensor (DO)  │
│                                 │
│  GPIO 13 ──→ Servo (Cửa chính) │
│  GPIO 25 ──→ Servo (Cửa sổ 1)   │
│  GPIO 26 ──→ Servo (Cửa sổ 2)   │
│  GPIO 12 ──→ Buzzer             │
│  GPIO 14 ──→ LED Báo động        │
│  GPIO 27 ──→ LED Thông minh      │
│                                 │
│  3.3V ────→ Power (Servos)     │
│  GND ─────→ Ground              │
└─────────────────────────────────┘
```

### Kết nối cảm biến khí gas (MQ-2)
- **VCC** → 5V
- **GND** → GND
- **A0** → GPIO 34 (Analog)

### Kết nối cảm biến ánh sáng
- **VCC** → 3.3V
- **GND** → GND
- **DO** → GPIO 35 (Digital)

### Kết nối Servo
- **Red (VCC)** → 5V (hoặc nguồn ngoài nếu cần)
- **Black (GND)** → GND
- **Yellow/Orange (Signal)** → GPIO tương ứng

### Kết nối Buzzer
- **Positive** → GPIO 12
- **Negative** → GND

### Kết nối LED
- **Anode (+)** → GPIO tương ứng (qua resistor 220Ω)
- **Cathode (-)** → GND

## 🐛 Troubleshooting

### ESP32 không kết nối WiFi
- Kiểm tra SSID và password trong code
- Đảm bảo WiFi 2.4GHz (ESP32 không hỗ trợ 5GHz)
- Kiểm tra khoảng cách đến router

### ESP32 không kết nối Firebase
- Kiểm tra Firebase Host và Auth token
- Kiểm tra Security Rules trong Firebase Console
- Đảm bảo ESP32 đã kết nối WiFi thành công

### Web dashboard không hiển thị dữ liệu
- Kiểm tra trạng thái kết nối (chấm màu xanh/đỏ)
- Mở Console (F12) để xem lỗi
- Kiểm tra Firebase config trong `index.html`

### Python face recognition không chạy
- Kiểm tra webcam có hoạt động không
- Đảm bảo đã cài đặt đầy đủ thư viện
- Kiểm tra đường dẫn ảnh và Firebase key
- Trên Windows, có thể cần cài Visual C++ Build Tools

### Face recognition không chính xác
- Train nhiều ảnh với góc độ khác nhau
- Đảm bảo ánh sáng đủ khi train và nhận diện
- Điều chỉnh `FACE_TOLERANCE` và `FACE_DISTANCE_THRESHOLD` nếu cần

### Cửa không mở khi nhận diện
- Kiểm tra ESP32 có nhận được lệnh từ Firebase không
- Kiểm tra Servo có được cấp nguồn đủ không
- Xem Serial Monitor của ESP32 để debug

## 📝 Ghi chú

- **Hysteresis**: Hệ thống sử dụng hysteresis (900-1000) để tránh dao động khi giá trị gas ở gần ngưỡng
- **Cooldown**: Có cơ chế cooldown để tránh spam lệnh
- **Non-blocking**: ESP32 code được tối ưu để không block, đảm bảo phản hồi nhanh
- **Real-time**: Tất cả dữ liệu được đồng bộ real-time qua Firebase

## 📄 License

Dự án này được phát triển cho mục đích giáo dục và nghiên cứu.

## 👨‍💻 Tác giả

Phát triển bởi SmartHome IoT Team

---

**Lưu ý**: Đảm bảo thay đổi thông tin WiFi và Firebase credentials trước khi sử dụng!
