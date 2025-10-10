"""
Smart Home IoT System
Hệ thống Nhà thông minh sử dụng công nghệ IoT để điều khiển và giám sát các thiết bị
"""

from abc import ABC, abstractmethod
from datetime import datetime
from typing import Dict, List, Optional
import json


class Device(ABC):
    """Base class for all IoT devices"""
    
    def __init__(self, device_id: str, name: str, location: str):
        self.device_id = device_id
        self.name = name
        self.location = location
        self.is_online = True
        self.last_updated = datetime.now()
    
    @abstractmethod
    def get_status(self) -> Dict:
        """Get current status of the device"""
        pass
    
    @abstractmethod
    def execute_command(self, command: str, **kwargs) -> bool:
        """Execute a command on the device"""
        pass
    
    def get_info(self) -> Dict:
        """Get device information"""
        return {
            "device_id": self.device_id,
            "name": self.name,
            "location": self.location,
            "is_online": self.is_online,
            "last_updated": self.last_updated.isoformat()
        }


class Light(Device):
    """Smart Light device"""
    
    def __init__(self, device_id: str, name: str, location: str):
        super().__init__(device_id, name, location)
        self.is_on = False
        self.brightness = 0  # 0-100
        self.color = "white"
    
    def get_status(self) -> Dict:
        """Get light status"""
        return {
            **self.get_info(),
            "type": "Light",
            "is_on": self.is_on,
            "brightness": self.brightness,
            "color": self.color
        }
    
    def execute_command(self, command: str, **kwargs) -> bool:
        """Execute light command"""
        self.last_updated = datetime.now()
        
        if command == "turn_on":
            self.is_on = True
            if "brightness" in kwargs:
                self.brightness = max(0, min(100, kwargs["brightness"]))
            if "color" in kwargs:
                self.color = kwargs["color"]
            return True
        elif command == "turn_off":
            self.is_on = False
            self.brightness = 0
            return True
        elif command == "set_brightness":
            if self.is_on and "level" in kwargs:
                self.brightness = max(0, min(100, kwargs["level"]))
                return True
        elif command == "set_color":
            if "color" in kwargs:
                self.color = kwargs["color"]
                return True
        
        return False


class Thermostat(Device):
    """Smart Thermostat device"""
    
    def __init__(self, device_id: str, name: str, location: str):
        super().__init__(device_id, name, location)
        self.current_temp = 25.0  # Celsius
        self.target_temp = 22.0
        self.mode = "auto"  # auto, heat, cool, off
        self.is_on = True
    
    def get_status(self) -> Dict:
        """Get thermostat status"""
        return {
            **self.get_info(),
            "type": "Thermostat",
            "current_temp": self.current_temp,
            "target_temp": self.target_temp,
            "mode": self.mode,
            "is_on": self.is_on
        }
    
    def execute_command(self, command: str, **kwargs) -> bool:
        """Execute thermostat command"""
        self.last_updated = datetime.now()
        
        if command == "set_temperature":
            if "temp" in kwargs:
                self.target_temp = kwargs["temp"]
                return True
        elif command == "set_mode":
            if "mode" in kwargs and kwargs["mode"] in ["auto", "heat", "cool", "off"]:
                self.mode = kwargs["mode"]
                self.is_on = kwargs["mode"] != "off"
                return True
        elif command == "turn_on":
            self.is_on = True
            return True
        elif command == "turn_off":
            self.is_on = False
            self.mode = "off"
            return True
        
        return False


class SecurityCamera(Device):
    """Smart Security Camera device"""
    
    def __init__(self, device_id: str, name: str, location: str):
        super().__init__(device_id, name, location)
        self.is_recording = False
        self.motion_detected = False
        self.resolution = "1080p"
    
    def get_status(self) -> Dict:
        """Get camera status"""
        return {
            **self.get_info(),
            "type": "SecurityCamera",
            "is_recording": self.is_recording,
            "motion_detected": self.motion_detected,
            "resolution": self.resolution
        }
    
    def execute_command(self, command: str, **kwargs) -> bool:
        """Execute camera command"""
        self.last_updated = datetime.now()
        
        if command == "start_recording":
            self.is_recording = True
            return True
        elif command == "stop_recording":
            self.is_recording = False
            return True
        elif command == "set_resolution":
            if "resolution" in kwargs:
                self.resolution = kwargs["resolution"]
                return True
        
        return False


class DoorLock(Device):
    """Smart Door Lock device"""
    
    def __init__(self, device_id: str, name: str, location: str):
        super().__init__(device_id, name, location)
        self.is_locked = True
        self.last_access = None
    
    def get_status(self) -> Dict:
        """Get door lock status"""
        return {
            **self.get_info(),
            "type": "DoorLock",
            "is_locked": self.is_locked,
            "last_access": self.last_access.isoformat() if self.last_access else None
        }
    
    def execute_command(self, command: str, **kwargs) -> bool:
        """Execute door lock command"""
        self.last_updated = datetime.now()
        
        if command == "lock":
            self.is_locked = True
            self.last_access = datetime.now()
            return True
        elif command == "unlock":
            self.is_locked = False
            self.last_access = datetime.now()
            return True
        
        return False


class SmartHome:
    """Smart Home Controller - Main system to manage all IoT devices"""
    
    def __init__(self, name: str = "My Smart Home"):
        self.name = name
        self.devices: Dict[str, Device] = {}
    
    def add_device(self, device: Device) -> bool:
        """Add a device to the smart home system"""
        if device.device_id not in self.devices:
            self.devices[device.device_id] = device
            return True
        return False
    
    def remove_device(self, device_id: str) -> bool:
        """Remove a device from the smart home system"""
        if device_id in self.devices:
            del self.devices[device_id]
            return True
        return False
    
    def get_device(self, device_id: str) -> Optional[Device]:
        """Get a device by its ID"""
        return self.devices.get(device_id)
    
    def list_devices(self) -> List[Dict]:
        """List all devices in the system"""
        return [device.get_status() for device in self.devices.values()]
    
    def control_device(self, device_id: str, command: str, **kwargs) -> bool:
        """Control a device remotely"""
        device = self.get_device(device_id)
        if device:
            return device.execute_command(command, **kwargs)
        return False
    
    def get_devices_by_location(self, location: str) -> List[Device]:
        """Get all devices in a specific location"""
        return [device for device in self.devices.values() if device.location == location]
    
    def get_devices_by_type(self, device_type: str) -> List[Device]:
        """Get all devices of a specific type"""
        return [device for device in self.devices.values() 
                if type(device).__name__ == device_type]
    
    def get_system_status(self) -> Dict:
        """Get overall system status"""
        return {
            "name": self.name,
            "total_devices": len(self.devices),
            "online_devices": sum(1 for d in self.devices.values() if d.is_online),
            "devices": self.list_devices()
        }
    
    def export_config(self) -> str:
        """Export system configuration as JSON"""
        return json.dumps(self.get_system_status(), indent=2)


if __name__ == "__main__":
    # Example usage
    print("=== Smart Home IoT System Demo ===\n")
    
    # Create smart home instance
    home = SmartHome("Nhà thông minh của tôi")
    
    # Add devices
    living_room_light = Light("light_001", "Đèn phòng khách", "Phòng khách")
    bedroom_light = Light("light_002", "Đèn phòng ngủ", "Phòng ngủ")
    thermostat = Thermostat("thermo_001", "Điều hòa nhiệt độ", "Phòng khách")
    camera = SecurityCamera("cam_001", "Camera an ninh", "Cửa chính")
    door_lock = DoorLock("lock_001", "Khóa cửa chính", "Cửa chính")
    
    home.add_device(living_room_light)
    home.add_device(bedroom_light)
    home.add_device(thermostat)
    home.add_device(camera)
    home.add_device(door_lock)
    
    print(f"Đã thêm {len(home.devices)} thiết bị vào hệ thống\n")
    
    # Control devices
    print("1. Bật đèn phòng khách (độ sáng 80%):")
    home.control_device("light_001", "turn_on", brightness=80, color="warm white")
    print(f"   Trạng thái: {home.get_device('light_001').get_status()}\n")
    
    print("2. Đặt nhiệt độ điều hòa:")
    home.control_device("thermo_001", "set_temperature", temp=24)
    print(f"   Trạng thái: {home.get_device('thermo_001').get_status()}\n")
    
    print("3. Bắt đầu ghi hình camera:")
    home.control_device("cam_001", "start_recording")
    print(f"   Trạng thái: {home.get_device('cam_001').get_status()}\n")
    
    print("4. Mở khóa cửa:")
    home.control_device("lock_001", "unlock")
    print(f"   Trạng thái: {home.get_device('lock_001').get_status()}\n")
    
    # System status
    print("=== Trạng thái hệ thống ===")
    print(home.export_config())
