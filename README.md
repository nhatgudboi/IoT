# IoT - Smart Home System
Hệ thống Nhà thông minh (Smart Home IoT System)

## Giới thiệu / Introduction

Đây là một hệ thống Nhà thông minh sử dụng công nghệ IoT (Internet of Things) để điều khiển và giám sát các thiết bị trong ngôi nhà một cách tự động hoặc từ xa.

This is a Smart Home system using IoT (Internet of Things) technology to control and monitor devices in the house automatically or remotely.

## Tính năng / Features

- **Điều khiển thiết bị từ xa** / Remote device control
- **Giám sát trạng thái thiết bị** / Device status monitoring
- **Quản lý nhiều loại thiết bị** / Multi-device type management:
  - Đèn thông minh (Smart Lights)
  - Điều hòa nhiệt độ (Thermostats)
  - Camera an ninh (Security Cameras)
  - Khóa cửa thông minh (Smart Door Locks)
- **Phân loại thiết bị theo vị trí** / Device organization by location
- **Xuất cấu hình hệ thống** / System configuration export

## Cấu trúc dự án / Project Structure

```
IoT/
├── smart_home.py          # Main system implementation
├── test_smart_home.py     # Test suite
├── requirements.txt       # Python dependencies
└── README.md             # Documentation
```

## Yêu cầu hệ thống / Requirements

- Python 3.6 or higher
- No external dependencies required (uses Python standard library only)

## Cài đặt / Installation

```bash
# Clone repository
git clone https://github.com/nhatgudboi/IoT.git
cd IoT

# No additional installation needed - uses Python standard library
```

## Sử dụng / Usage

### Chạy demo cơ bản / Run basic demo

```bash
python3 smart_home.py
```

### Chạy test / Run tests

```bash
python3 -m unittest test_smart_home.py -v
```

### Sử dụng trong code / Use in code

```python
from smart_home import SmartHome, Light, Thermostat, SecurityCamera, DoorLock

# Tạo hệ thống nhà thông minh / Create smart home system
home = SmartHome("Nhà của tôi")

# Thêm thiết bị / Add devices
living_room_light = Light("light_001", "Đèn phòng khách", "Phòng khách")
thermostat = Thermostat("thermo_001", "Điều hòa", "Phòng khách")
camera = SecurityCamera("cam_001", "Camera cửa chính", "Cửa chính")
door_lock = DoorLock("lock_001", "Khóa cửa chính", "Cửa chính")

home.add_device(living_room_light)
home.add_device(thermostat)
home.add_device(camera)
home.add_device(door_lock)

# Điều khiển thiết bị / Control devices
home.control_device("light_001", "turn_on", brightness=80)
home.control_device("thermo_001", "set_temperature", temp=24)
home.control_device("cam_001", "start_recording")
home.control_device("lock_001", "unlock")

# Kiểm tra trạng thái / Check status
status = home.get_system_status()
print(status)

# Xuất cấu hình / Export configuration
config = home.export_config()
print(config)
```

## Các loại thiết bị / Device Types

### 1. Light (Đèn thông minh)
**Commands:**
- `turn_on` - Bật đèn (Turn on)
  - Parameters: `brightness` (0-100), `color`
- `turn_off` - Tắt đèn (Turn off)
- `set_brightness` - Đặt độ sáng (Set brightness)
  - Parameters: `level` (0-100)
- `set_color` - Đặt màu (Set color)
  - Parameters: `color`

### 2. Thermostat (Điều hòa nhiệt độ)
**Commands:**
- `set_temperature` - Đặt nhiệt độ (Set temperature)
  - Parameters: `temp` (Celsius)
- `set_mode` - Đặt chế độ (Set mode)
  - Parameters: `mode` (auto, heat, cool, off)
- `turn_on` - Bật (Turn on)
- `turn_off` - Tắt (Turn off)

### 3. SecurityCamera (Camera an ninh)
**Commands:**
- `start_recording` - Bắt đầu ghi hình (Start recording)
- `stop_recording` - Dừng ghi hình (Stop recording)
- `set_resolution` - Đặt độ phân giải (Set resolution)
  - Parameters: `resolution` (e.g., "1080p", "4K")

### 4. DoorLock (Khóa cửa thông minh)
**Commands:**
- `lock` - Khóa cửa (Lock door)
- `unlock` - Mở khóa (Unlock door)

## API Reference

### SmartHome Class

- `add_device(device)` - Thêm thiết bị vào hệ thống
- `remove_device(device_id)` - Xóa thiết bị khỏi hệ thống
- `get_device(device_id)` - Lấy thiết bị theo ID
- `list_devices()` - Liệt kê tất cả thiết bị
- `control_device(device_id, command, **kwargs)` - Điều khiển thiết bị từ xa
- `get_devices_by_location(location)` - Lấy thiết bị theo vị trí
- `get_devices_by_type(device_type)` - Lấy thiết bị theo loại
- `get_system_status()` - Lấy trạng thái hệ thống
- `export_config()` - Xuất cấu hình dưới dạng JSON

## Ví dụ nâng cao / Advanced Examples

### Tự động hóa theo vị trí / Location-based automation

```python
# Tắt tất cả đèn trong phòng khách
living_room_lights = home.get_devices_by_location("Phòng khách")
for device in living_room_lights:
    if isinstance(device, Light):
        device.execute_command("turn_off")
```

### Kiểm tra trạng thái toàn bộ hệ thống / Check entire system status

```python
status = home.get_system_status()
print(f"Tổng số thiết bị: {status['total_devices']}")
print(f"Thiết bị online: {status['online_devices']}")
```

## Đóng góp / Contributing

Mọi đóng góp đều được chào đón! Vui lòng tạo Pull Request hoặc mở Issue để đề xuất cải tiến.

All contributions are welcome! Please create a Pull Request or open an Issue to suggest improvements.

## Giấy phép / License

MIT License

## Liên hệ / Contact

GitHub: [nhatgudboi](https://github.com/nhatgudboi)
