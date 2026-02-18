import argparse
import sys
import time
import serial
import serial.tools.list_ports
import os
import subprocess

def list_ports():
    ports = serial.tools.list_ports.comports()
    return [port.device for port in ports]

def select_port():
    ports = serial.tools.list_ports.comports()
    
    # Filter out likely irrelevant ports (standard PC serial ports)
    # On Linux, these are usually ttyS* which are rarely the ESP32
    candidates = []
    for p in ports:
        # Keep it if it's NOT ttyS* OR if we are not on linux
        if not (sys.platform.startswith("linux") and "ttyS" in p.device):
            candidates.append(p)
            
    # If we filtered everything out, revert to showing everything
    if not candidates and ports:
        candidates = ports

    if not candidates:
        print("No serial ports found!")
        return None

    # If only one candidate found, auto-select it
    if len(candidates) == 1:
        print(f"Auto-selected port: {candidates[0].device} ({candidates[0].description})")
        return candidates[0].device
    
    print("\nAvailable ports:")
    for i, port in enumerate(candidates):
        print(f"[{i}] {port.device} - {port.description}")
    
    try:
        selection_input = input("\nSelect port number: ")
        if not selection_input.strip(): # Default to 0 if enter pressed
             print(f"Selected: {candidates[0].device}")
             return candidates[0].device
             
        selection = int(selection_input)
        if 0 <= selection < len(candidates):
            return candidates[selection].device
        else:
            print("Invalid selection.")
            return None
    except (ValueError, IndexError):
        print("Invalid selection.")
        return None

def send_command(ser, command):
    print(f"Sending: {command}")
    ser.write(f"{command}\n".encode())
    time.sleep(0.5)
    while ser.in_waiting:
        print(ser.readline().decode().strip())

def setup(port, config_file=None):
    if not port:
        port = select_port()
        if not port: return

    ssid = ""
    password = ""
    tg_token = ""
    api_key = ""

    if config_file:
        try:
            import json
            with open(config_file, 'r') as f:
                config = json.load(f)
                ssid = config.get("wifi_ssid", "")
                password = config.get("wifi_password", "")
                tg_token = config.get("telegram_token", "")
                api_key = config.get("gemini_key", "")
                print(f"Loaded configuration from {config_file}")
        except Exception as e:
            print(f"Error reading config file: {e}")
            return
    else:
        # Interactive Setup
        print("\n--- MicroClaw Setup Wizard ---\n")
        ssid = input("Enter Wi-Fi SSID: ")
        password = input("Enter Wi-Fi Password: ")
        tg_token = input("Enter Telegram Bot Token: ")
        api_key = input("Enter Gemini API Key: ")
    
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        print(f"Connected to {port}. Waiting for device...")
        time.sleep(2) # Wait for reset
        
        print("\nConfiguring device...")
        if ssid: 
            send_command(ser, f"wifi_set {ssid} {password}")
            time.sleep(1)
        if tg_token:
            send_command(ser, f"set_tg_token {tg_token}")
            time.sleep(1)
        if api_key:
            send_command(ser, f"set_api_key {api_key}")
            time.sleep(1)
        
        print("\nConfiguration sent! Rebooting device...")
        send_command(ser, "restart")
        
        ser.close()
        print("Done!")

    except Exception as e:
        print(f"Error: {e}")

def monitor(port):
    if not port:
        port = select_port()
        if not port: return

    print(f"Starting monitor on {port} (Ctrl+C to exit)...")
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        while True:
            if ser.in_waiting:
                try:
                    line = ser.readline().decode('utf-8', errors='replace').strip()
                    if line: print(line)
                except Exception:
                    pass
    except KeyboardInterrupt:
        print("\nExiting monitor.")
    except Exception as e:
        print(f"Error: {e}")

def flash(port, firmware_path):
    if not port:
        port = select_port()
        if not port: return
    
    if not os.path.exists(firmware_path):
        print(f"Firmware file not found: {firmware_path}")
        return

    print(f"Flashing {firmware_path} to {port}...")
    # Using esptool.py via subprocess
    cmd = [
        sys.executable, "-m", "esptool",
        "--chip", "esp32",
        "--port", port,
        "--baud", "460800",
        "--before", "default_reset",
        "--after", "hard_reset",
        "write_flash",
        "-z",
        "--flash_mode", "dio",
        "--flash_freq", "40m",
        "--flash_size", "detect",
        "0x1000", "bootloader_dio_40m.bin", # Placeholder offsets, user needs full bin or separate bins
        "0x8000", "partitions.bin",
        "0x10000", firmware_path
    ]
    
    # For simplicity, if we have a single merged bin (common in production), we just flash to 0x0
    # or if we are using PlatformIO generated bin:
    cmd_simple = [
        sys.executable, "-m", "esptool",
        "--chip", "esp32",
        "--port", port,
        "--baud", "460800",
        "write_flash", "0x10000", firmware_path
    ]

    subprocess.run(cmd_simple)

def main():
    parser = argparse.ArgumentParser(description="MicroClaw CLI Tool")
    subparsers = parser.add_subparsers(dest="command")

    # Setup Command
    setup_parser = subparsers.add_parser("setup", help="Configure Wi-Fi and Keys")
    setup_parser.add_argument("--port", "-p", help="Serial port")
    setup_parser.add_argument("--config", "-c", help="Path to config.json")

    # Monitor Command
    monitor_parser = subparsers.add_parser("monitor", help="Monitor Serial Output")
    monitor_parser.add_argument("--port", "-p", help="Serial port")
    
    # Flash Command
    flash_parser = subparsers.add_parser("flash", help="Flash Firmware")
    flash_parser.add_argument("--port", "-p", help="Serial port")
    flash_parser.add_argument("--firmware", "-f", default="../firmware/.pio/build/esp32dev/firmware.bin", help="Path to firmware.bin")

    args = parser.parse_args()

    if args.command == "setup":
        setup(args.port, args.config)
    elif args.command == "monitor":
        monitor(args.port)
    elif args.command == "flash":
        flash(args.port, args.firmware)
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
