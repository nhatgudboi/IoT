"""
Example usage of Smart Home IoT System
This file demonstrates various use cases and scenarios
"""

from smart_home import SmartHome, Light, Thermostat, SecurityCamera, DoorLock


def setup_home():
    """Set up a complete smart home system"""
    home = SmartHome("My Smart Home")
    
    # Living Room devices
    home.add_device(Light("light_lr_001", "Living Room Main Light", "Living Room"))
    home.add_device(Light("light_lr_002", "Living Room Accent Light", "Living Room"))
    home.add_device(Thermostat("thermo_lr_001", "Living Room Thermostat", "Living Room"))
    
    # Bedroom devices
    home.add_device(Light("light_br_001", "Bedroom Light", "Bedroom"))
    home.add_device(Thermostat("thermo_br_001", "Bedroom Thermostat", "Bedroom"))
    
    # Security devices
    home.add_device(SecurityCamera("cam_front", "Front Door Camera", "Front Door"))
    home.add_device(SecurityCamera("cam_back", "Back Door Camera", "Back Door"))
    home.add_device(DoorLock("lock_front", "Front Door Lock", "Front Door"))
    home.add_device(DoorLock("lock_back", "Back Door Lock", "Back Door"))
    
    # Kitchen devices
    home.add_device(Light("light_kitchen", "Kitchen Light", "Kitchen"))
    
    return home


def scenario_leave_home(home):
    """Scenario: Leaving home - secure and save energy"""
    print("=== Leaving Home Scenario ===")
    
    # Turn off all lights
    lights = home.get_devices_by_type("Light")
    for light in lights:
        home.control_device(light.device_id, "turn_off")
        print(f"Turned off: {light.name}")
    
    # Set thermostats to eco mode
    thermostats = home.get_devices_by_type("Thermostat")
    for thermo in thermostats:
        home.control_device(thermo.device_id, "set_mode", mode="cool")
        home.control_device(thermo.device_id, "set_temperature", temp=28)
        print(f"Set eco mode: {thermo.name}")
    
    # Lock all doors
    locks = home.get_devices_by_type("DoorLock")
    for lock in locks:
        home.control_device(lock.device_id, "lock")
        print(f"Locked: {lock.name}")
    
    # Start recording on all cameras
    cameras = home.get_devices_by_type("SecurityCamera")
    for camera in cameras:
        home.control_device(camera.device_id, "start_recording")
        print(f"Started recording: {camera.name}")
    
    print("All systems secured!\n")


def scenario_arrive_home(home):
    """Scenario: Arriving home - welcome setup"""
    print("=== Arriving Home Scenario ===")
    
    # Unlock front door
    home.control_device("lock_front", "unlock")
    print("Front door unlocked")
    
    # Turn on living room lights
    home.control_device("light_lr_001", "turn_on", brightness=70, color="warm white")
    print("Living room lights on")
    
    # Set comfortable temperature
    home.control_device("thermo_lr_001", "set_mode", mode="auto")
    home.control_device("thermo_lr_001", "set_temperature", temp=23)
    print("Thermostat set to comfort mode")
    
    # Stop camera recording (you're home)
    cameras = home.get_devices_by_type("SecurityCamera")
    for camera in cameras:
        home.control_device(camera.device_id, "stop_recording")
    print("Cameras stopped recording")
    
    print("Welcome home!\n")


def scenario_night_mode(home):
    """Scenario: Night mode - prepare for sleep"""
    print("=== Night Mode Scenario ===")
    
    # Dim bedroom light
    home.control_device("light_br_001", "turn_on", brightness=20, color="warm")
    print("Bedroom light dimmed")
    
    # Turn off other lights
    home.control_device("light_lr_001", "turn_off")
    home.control_device("light_lr_002", "turn_off")
    home.control_device("light_kitchen", "turn_off")
    print("Other lights turned off")
    
    # Lock all doors
    locks = home.get_devices_by_type("DoorLock")
    for lock in locks:
        home.control_device(lock.device_id, "lock")
    print("All doors locked")
    
    # Set bedroom to sleep temperature
    home.control_device("thermo_br_001", "set_temperature", temp=20)
    print("Bedroom temperature lowered")
    
    # Enable all cameras
    cameras = home.get_devices_by_type("SecurityCamera")
    for camera in cameras:
        home.control_device(camera.device_id, "start_recording")
    print("Security cameras active")
    
    print("Good night!\n")


def scenario_movie_time(home):
    """Scenario: Movie time in living room"""
    print("=== Movie Time Scenario ===")
    
    # Dim living room lights
    home.control_device("light_lr_001", "turn_on", brightness=30, color="blue")
    home.control_device("light_lr_002", "turn_off")
    print("Lights adjusted for movie watching")
    
    # Set comfortable temperature
    home.control_device("thermo_lr_001", "set_temperature", temp=22)
    print("Temperature set for comfort")
    
    print("Enjoy your movie!\n")


def main():
    """Main demonstration"""
    print("=" * 60)
    print("Smart Home IoT System - Example Scenarios")
    print("=" * 60 + "\n")
    
    # Set up home
    home = setup_home()
    print(f"Smart home set up with {len(home.devices)} devices\n")
    
    # Run scenarios
    scenario_arrive_home(home)
    scenario_movie_time(home)
    scenario_night_mode(home)
    scenario_leave_home(home)
    
    # Display final system status
    print("=== Final System Status ===")
    status = home.get_system_status()
    print(f"Total devices: {status['total_devices']}")
    print(f"Online devices: {status['online_devices']}")
    
    # Show devices by location
    print("\nDevices by location:")
    locations = set(device.location for device in home.devices.values())
    for location in sorted(locations):
        devices = home.get_devices_by_location(location)
        print(f"  {location}: {len(devices)} device(s)")
    
    # Export configuration
    print("\n=== System Configuration (JSON) ===")
    print(home.export_config())


if __name__ == "__main__":
    main()
