import argparse
import sys
import time
import serial
import serial.tools.list_ports
import os
import subprocess
import shutil

# Try to import Rich, otherwise fallback to print
try:
    from rich.console import Console
    from rich.panel import Panel
    from rich.prompt import Prompt, Confirm
    from rich import print as rprint
    console = Console()
    HAS_RICH = True
except ImportError:
    HAS_RICH = False
    class Console:
        def print(self, *args, **kwargs): print(*args)
        def rule(self, *args, **kwargs): print("-" * 50)
    class Prompt:
        @staticmethod
        def ask(prompt, **kwargs): 
            default = kwargs.get("default")
            p = prompt if not default else f"{prompt} [{default}]"
            return input(p + ": ") or default
    class Confirm:
        @staticmethod
        def ask(prompt, **kwargs):
            return input(prompt + " (y/n): ").lower().startswith('y')
    def rprint(*args, **kwargs): print(*args)
    console = Console()

def get_current_wifi_ssid():
    """Attempts to get current SSID on Linux using nmcli"""
    if sys.platform.startswith("linux"):
        try:
            # Check if nmcli exists
            if not shutil.which("nmcli"):
                return None
            
            # Run nmcli to get active connection
            cmd = "nmcli -t -f active,ssid dev wifi | grep '^yes' | cut -d: -f2"
            result = subprocess.check_output(cmd, shell=True).decode().strip()
            return result if result else None
        except Exception:
            return None
    return None

def validate_gemini_key(api_key):
    """Checks if the Gemini API key is valid using a simple model call."""
    import json
    import urllib.request
    import urllib.error

    url = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key={api_key}"
    data = json.dumps({"contents": [{"parts": [{"text": "Hello"}]}]}).encode('utf-8')
    req = urllib.request.Request(url, data=data, headers={'Content-Type': 'application/json'})

    try:
        with urllib.request.urlopen(req) as response:
            if response.getcode() == 200:
                result = json.loads(response.read().decode())
                # Just check if we got a candidate
                if "candidates" in result:
                     return True, "Key is valid!"
                return False, "Key valid but unexpected response format."
    except urllib.error.HTTPError as e:
        return False, f"HTTP Error {e.code}: {e.reason}"
    except Exception as e:
        return False, f"Error: {e}"

def get_current_wifi_ssid():
    """Attempts to get current SSID on Linux using nmcli"""
    if sys.platform.startswith("linux"):
        try:
            # Check if nmcli exists
            if not shutil.which("nmcli"):
                return None
            
            # Run nmcli to get active connection
            cmd = "nmcli -t -f active,ssid dev wifi | grep '^yes' | cut -d: -f2"
            result = subprocess.check_output(cmd, shell=True).decode().strip()
            return result if result else None
        except Exception:
            return None
    return None

def list_ports():
    ports = serial.tools.list_ports.comports()
    return [port.device for port in ports]

def select_port():
    ports = serial.tools.list_ports.comports()
    
    # Filter out likely irrelevant ports
    candidates = []
    for p in ports:
        if not (sys.platform.startswith("linux") and "ttyS" in p.device):
            candidates.append(p)
    if not candidates and ports: candidates = ports

    if not candidates:
        console.print("[red]No serial ports found![/red]")
        return None

    if len(candidates) == 1:
        console.print(f"[green]Auto-selected port:[/green] {candidates[0].device} ({candidates[0].description})")
        return candidates[0].device
    
    console.print("\n[bold cyan]Available ports:[/bold cyan]")
    for i, port in enumerate(candidates):
        console.print(f"[{i}] {port.device} - {port.description}")
    
    try:
        selection_input = Prompt.ask("\nSelect port number", default="0")
        selection = int(selection_input)
        if 0 <= selection < len(candidates):
            return candidates[selection].device
        else:
            console.print("[red]Invalid selection.[/red]")
            return None
    except (ValueError, IndexError):
        console.print("[red]Invalid selection.[/red]")
        return None

def send_command(ser, command):
    console.print(f"[dim]Sending: {command}[/dim]")
    ser.write(f"{command}\n".encode())
    time.sleep(0.5)
    while ser.in_waiting:
        line = ser.readline().decode('utf-8', errors='replace').strip()
        console.print(f"[blue]Device:[/blue] {line}")

def setup(port, config_file=None):
    console.rule("[bold green]MicroClaw Setup Wizard[/bold green]")
    
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
                console.print(f"[green]Loaded configuration from {config_file}[/green]")
                
                if api_key:
                    with console.status("[bold green]Validating API Key from config...[/bold green]"):
                        is_valid, msg = validate_gemini_key(api_key)
                    if is_valid:
                         console.print(f"[green]✓ {msg}[/green]")
                    else:
                         console.print(f"[red]✗ Invalid Key in config: {msg}[/red]")
                         if not Confirm.ask("Proceed anyway?"):
                             return
        except Exception as e:
            console.print(f"[red]Error reading config file: {e}[/red]")
            return
    else:
        # Interactive Setup with Rich Prompt
        
        # 1. Wi-Fi
        current_ssid = get_current_wifi_ssid()
        default_ssid = current_ssid if current_ssid else ""
        
        console.print(Panel("Enter your Wi-Fi credentials. (2.4GHz only!)", title="Wi-Fi Configuration"))
        ssid = Prompt.ask("Enter Wi-Fi SSID", default=default_ssid)
        password = Prompt.ask("Enter Wi-Fi Password", password=True)
        
        # 2. Telegram
        console.print(Panel("Enter your Telegram Bot Token from @BotFather.\n[dim]Optional: Press Enter to skip if you only want local chat.[/dim]", title="Telegram Configuration"))
        tg_token = Prompt.ask("Telegram Bot Token", default="")
        
        # 3. Gemini
        console.print(Panel("Enter your Google Gemini API Key from aistudio.google.com.", title="AI Configuration"))
        while True:
            api_key = Prompt.ask("Gemini API Key")
            if not api_key: break
            
            with console.status("[bold green]Validating API Key...[/bold green]"):
                is_valid, msg = validate_gemini_key(api_key)
            
            if is_valid:
                console.print(f"[green]✓ {msg}[/green]")
                break
            else:
                console.print(f"[red]✗ Invalid Key: {msg}[/red]")
                if not Confirm.ask("Use this key anyway?"):
                    continue
                break

    # Confirmation
    if not Confirm.ask(f"\nReady to configure device on {port}?"):
        console.print("[yellow]Cancelled.[/yellow]")
        return

    try:
        ser = serial.Serial(port, 115200, timeout=1)
        console.print(f"[green]Connected to {port}. Waiting for device reset...[/green]")
        time.sleep(2) 
        
        console.print("\n[bold]Configuring device...[/bold]")
        if ssid: 
            send_command(ser, f'wifi_set "{ssid}" "{password}"')
            time.sleep(1)
        if tg_token:
            send_command(ser, f'set_tg_token "{tg_token}"')
            time.sleep(1)
        if api_key:
            url = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key={api_key}"
            send_command(ser, f'set_gemini_config "{api_key}" "{url}"')
            time.sleep(1)
        
        console.print("\n[bold green]Configuration sent! Rebooting device...[/bold green]")
        send_command(ser, "restart")
        
        ser.close()
        console.print(Panel("[bold green]Setup Complete![/bold green]\n\nRun 'monitor' to see device logs or open Web UI.", border_style="green"))

    except Exception as e:
        if "Permission denied" in str(e):
            console.print(Panel("[red]Permission Denied![/red]\n\nYou need to add your user to the 'dialout' group.\nRun: [bold]sudo usermod -a -G dialout $USER[/bold] and then [bold]LOG OUT & LOGIN[/bold].\n\n[yellow]Quick Fix:[/yellow] Run this tool with [bold]sudo[/bold].", border_style="red"))
        console.print(f"[red]Error: {e}[/red]")

def scan_wifi(port):
    if not port:
        port = select_port()
        if not port: return

    try:
        ser = serial.Serial(port, 115200, timeout=1)
        console.print(f"[green]Scanning for networks on {port}...[/green]")
        send_command(ser, "wifi_scan")
        
        # Wait for scan result
        start = time.time()
        while time.time() - start < 10:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if line: console.print(f"[blue]Device:[/blue] {line}")
            time.sleep(0.1)
            
    except Exception as e:
        console.print(f"[red]Error: {e}[/red]")

def monitor(port):
    if not port:
        port = select_port()
        if not port: return

    console.print(f"[green]Starting monitor on {port} (Ctrl+C to exit)...[/green]")
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
        console.print("\n[yellow]Exiting monitor.[/yellow]")
    except Exception as e:
        console.print(f"[red]Error: {e}[/red]")

def flash(port, firmware_path):
    if not port:
        port = select_port()
        if not port: return
    
    if not os.path.exists(firmware_path):
        console.print(f"[red]Firmware file not found: {firmware_path}[/red]")
        return

    console.print(f"[bold blue]Flashing {firmware_path} to {port}...[/bold blue]")
    
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

    # Scan Command
    subparsers.add_parser("scan", help="Scan for Wi-Fi Networks")

    args = parser.parse_args()

    if args.command == "setup":
        setup(args.port, args.config)
    elif args.command == "monitor":
        monitor(args.port)
    elif args.command == "flash":
        flash(args.port, args.firmware)
    elif args.command == "scan":
        scan_wifi(args.port)
    else:
        parser.print_help()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nOperation cancelled by user. Exiting...")
        sys.exit(0)
