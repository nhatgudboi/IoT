"""
Test suite for Smart Home IoT System
"""

import unittest
from datetime import datetime
from smart_home import (
    Device, Light, Thermostat, SecurityCamera, DoorLock, SmartHome
)


class TestLight(unittest.TestCase):
    """Test Light device"""
    
    def setUp(self):
        self.light = Light("test_light_001", "Test Light", "Test Room")
    
    def test_initial_state(self):
        """Test initial state of light"""
        self.assertFalse(self.light.is_on)
        self.assertEqual(self.light.brightness, 0)
        self.assertEqual(self.light.color, "white")
    
    def test_turn_on(self):
        """Test turning light on"""
        result = self.light.execute_command("turn_on", brightness=50)
        self.assertTrue(result)
        self.assertTrue(self.light.is_on)
        self.assertEqual(self.light.brightness, 50)
    
    def test_turn_off(self):
        """Test turning light off"""
        self.light.execute_command("turn_on", brightness=80)
        result = self.light.execute_command("turn_off")
        self.assertTrue(result)
        self.assertFalse(self.light.is_on)
        self.assertEqual(self.light.brightness, 0)
    
    def test_set_brightness(self):
        """Test setting brightness"""
        self.light.execute_command("turn_on")
        result = self.light.execute_command("set_brightness", level=75)
        self.assertTrue(result)
        self.assertEqual(self.light.brightness, 75)
    
    def test_set_color(self):
        """Test setting color"""
        result = self.light.execute_command("set_color", color="blue")
        self.assertTrue(result)
        self.assertEqual(self.light.color, "blue")
    
    def test_get_status(self):
        """Test getting status"""
        status = self.light.get_status()
        self.assertEqual(status["type"], "Light")
        self.assertIn("device_id", status)
        self.assertIn("is_on", status)


class TestThermostat(unittest.TestCase):
    """Test Thermostat device"""
    
    def setUp(self):
        self.thermostat = Thermostat("test_thermo_001", "Test Thermostat", "Test Room")
    
    def test_initial_state(self):
        """Test initial state of thermostat"""
        self.assertTrue(self.thermostat.is_on)
        self.assertEqual(self.thermostat.mode, "auto")
        self.assertEqual(self.thermostat.target_temp, 22.0)
    
    def test_set_temperature(self):
        """Test setting temperature"""
        result = self.thermostat.execute_command("set_temperature", temp=25)
        self.assertTrue(result)
        self.assertEqual(self.thermostat.target_temp, 25)
    
    def test_set_mode(self):
        """Test setting mode"""
        result = self.thermostat.execute_command("set_mode", mode="cool")
        self.assertTrue(result)
        self.assertEqual(self.thermostat.mode, "cool")
        self.assertTrue(self.thermostat.is_on)
    
    def test_turn_off(self):
        """Test turning thermostat off"""
        result = self.thermostat.execute_command("turn_off")
        self.assertTrue(result)
        self.assertFalse(self.thermostat.is_on)
        self.assertEqual(self.thermostat.mode, "off")
    
    def test_get_status(self):
        """Test getting status"""
        status = self.thermostat.get_status()
        self.assertEqual(status["type"], "Thermostat")
        self.assertIn("current_temp", status)
        self.assertIn("target_temp", status)


class TestSecurityCamera(unittest.TestCase):
    """Test Security Camera device"""
    
    def setUp(self):
        self.camera = SecurityCamera("test_cam_001", "Test Camera", "Test Location")
    
    def test_initial_state(self):
        """Test initial state of camera"""
        self.assertFalse(self.camera.is_recording)
        self.assertFalse(self.camera.motion_detected)
        self.assertEqual(self.camera.resolution, "1080p")
    
    def test_start_recording(self):
        """Test starting recording"""
        result = self.camera.execute_command("start_recording")
        self.assertTrue(result)
        self.assertTrue(self.camera.is_recording)
    
    def test_stop_recording(self):
        """Test stopping recording"""
        self.camera.execute_command("start_recording")
        result = self.camera.execute_command("stop_recording")
        self.assertTrue(result)
        self.assertFalse(self.camera.is_recording)
    
    def test_set_resolution(self):
        """Test setting resolution"""
        result = self.camera.execute_command("set_resolution", resolution="4K")
        self.assertTrue(result)
        self.assertEqual(self.camera.resolution, "4K")


class TestDoorLock(unittest.TestCase):
    """Test Door Lock device"""
    
    def setUp(self):
        self.lock = DoorLock("test_lock_001", "Test Lock", "Test Door")
    
    def test_initial_state(self):
        """Test initial state of lock"""
        self.assertTrue(self.lock.is_locked)
        self.assertIsNone(self.lock.last_access)
    
    def test_unlock(self):
        """Test unlocking"""
        result = self.lock.execute_command("unlock")
        self.assertTrue(result)
        self.assertFalse(self.lock.is_locked)
        self.assertIsNotNone(self.lock.last_access)
    
    def test_lock(self):
        """Test locking"""
        self.lock.execute_command("unlock")
        result = self.lock.execute_command("lock")
        self.assertTrue(result)
        self.assertTrue(self.lock.is_locked)
        self.assertIsNotNone(self.lock.last_access)


class TestSmartHome(unittest.TestCase):
    """Test Smart Home Controller"""
    
    def setUp(self):
        self.home = SmartHome("Test Home")
        self.light = Light("light_001", "Test Light", "Living Room")
        self.thermostat = Thermostat("thermo_001", "Test Thermostat", "Living Room")
    
    def test_add_device(self):
        """Test adding device"""
        result = self.home.add_device(self.light)
        self.assertTrue(result)
        self.assertEqual(len(self.home.devices), 1)
    
    def test_add_duplicate_device(self):
        """Test adding duplicate device"""
        self.home.add_device(self.light)
        result = self.home.add_device(self.light)
        self.assertFalse(result)
        self.assertEqual(len(self.home.devices), 1)
    
    def test_remove_device(self):
        """Test removing device"""
        self.home.add_device(self.light)
        result = self.home.remove_device("light_001")
        self.assertTrue(result)
        self.assertEqual(len(self.home.devices), 0)
    
    def test_get_device(self):
        """Test getting device"""
        self.home.add_device(self.light)
        device = self.home.get_device("light_001")
        self.assertIsNotNone(device)
        self.assertEqual(device.device_id, "light_001")
    
    def test_control_device(self):
        """Test controlling device"""
        self.home.add_device(self.light)
        result = self.home.control_device("light_001", "turn_on", brightness=70)
        self.assertTrue(result)
        self.assertTrue(self.light.is_on)
        self.assertEqual(self.light.brightness, 70)
    
    def test_list_devices(self):
        """Test listing devices"""
        self.home.add_device(self.light)
        self.home.add_device(self.thermostat)
        devices = self.home.list_devices()
        self.assertEqual(len(devices), 2)
    
    def test_get_devices_by_location(self):
        """Test getting devices by location"""
        self.home.add_device(self.light)
        self.home.add_device(self.thermostat)
        devices = self.home.get_devices_by_location("Living Room")
        self.assertEqual(len(devices), 2)
    
    def test_get_devices_by_type(self):
        """Test getting devices by type"""
        self.home.add_device(self.light)
        self.home.add_device(self.thermostat)
        lights = self.home.get_devices_by_type("Light")
        self.assertEqual(len(lights), 1)
        self.assertIsInstance(lights[0], Light)
    
    def test_get_system_status(self):
        """Test getting system status"""
        self.home.add_device(self.light)
        status = self.home.get_system_status()
        self.assertEqual(status["name"], "Test Home")
        self.assertEqual(status["total_devices"], 1)
        self.assertIn("devices", status)
    
    def test_export_config(self):
        """Test exporting configuration"""
        self.home.add_device(self.light)
        config = self.home.export_config()
        self.assertIsInstance(config, str)
        self.assertIn("Test Home", config)


if __name__ == "__main__":
    unittest.main()
