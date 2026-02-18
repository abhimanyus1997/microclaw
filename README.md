# MicroClaw: AI-Powered ESP32 Agent

<p align="center">
  <strong>The open-source, affordable AI agent for ESP32</strong>
</p>

MicroClaw turns a standard ESP32 development board into a physical AI agent. It connects to Wi-Fi, chats with you via Telegram, remembers your conversations, and controls a robotic claw to interact with the world.

## Features

- **🧠 Gemini AI Powered**: Uses Google's Gemini 1.5 Flash model for fast, intelligent responses.
- **💬 Telegram Integration**: Chat with your claw from anywhere in the world.
- **🔧 Tool Use**: The AI can autonomously control servos (Open, Close, Wave) and manage its own memory.
- **📁 Local Memory**: Remembers context across reboots using persistent file storage (`MEMORY.md`).
- **⚙️ CLI Configuration**: runtime configuration for Wi-Fi and API keys via Python CLI or Serial Monitor.
- **🔌 Cheap & Accessible**: Runs on a standard ESP32 ($5) with a simple servo ($2).

## Project Structure

```text
microclaw/
├── firmware/
│   ├── src/                # Source code (main.cpp)
│   ├── include/            # Header files (wifi, gemini, tools, etc.)
│   ├── lib/                # Dependencies
│   └── platformio.ini      # Build configuration
├── tools/
│   ├── microclaw.py        # Python CLI Tool
│   └── requirements.txt    # Python dependencies
├── config.example.json     # Sample configuration file
└── README.md               # Documentation
```

## Hardware Required

- **ESP32 Development Board** (e.g., ESP32 DevKit V1)
- **Servo Motor** (SG90 or MG996R)
  - Signal Wire -> GPIO 18
  - VCC -> 5V (or external power)
  - GND -> GND
- **Power Supply** (USB or Battery)

## Quick Start

### 1. Installation

**Prerequisites**: Python 3.x installed.

**Option A: Using pip (Standard)**
```bash
pip install -r tools/requirements.txt
```

**Option B: Using uv (Fast)**
[uv](https://github.com/astral-sh/uv) is an extremely fast Python package manager.

```bash
# 1. Install uv
curl -LsSf https://astral.sh/uv/install.sh | sh

# 2. Create virtual env and install dependencies
uv venv
uv pip install -r tools/requirements.txt

# OR 
uv add -r  tools/requirements.txt
```

**Option C: Run directly with uv**
You can also run the tool directly without manual installation:
```bash
uv run tools/microclaw.py setup
```

2.  **Flash Firmware** (Option A: Using CLI):
    If you have a pre-compiled binary:
    ```bash
    python tools/microclaw.py flash --firmware bin/firmware.bin
    ```

    **Flash Firmware** (Option B: Building from Source):
    - Install [PlatformIO](https://platformio.org/).
    - Open `firmware/` directory.
    - Run `pio run -t upload`.

### 2. Configuration

You can configure the device easily using the Python CLI tool.

**Option A: Burn from Config File (Recommended)**
1.  Copy the example config:
    ```bash
    cp config.example.json config.json
    ```
2.  Edit `config.json` with your credentials:
    ```json
    {
        "wifi_ssid": "MyWiFi",
        "wifi_password": "MyPassword",
        "telegram_token": "123456:ABC-DEF...",
        "gemini_key": "AIzaSy..."
    }
    ```
3.  Burn it to the device:
    ```bash
    python tools/microclaw.py setup --config config.json
    ```

**Option B: Interactive Wizard**
Run the tool without arguments to follow the guided setup:
```bash
python tools/microclaw.py setup
```

**Option C: Serial Monitor**
Open a serial terminal (baud 115200) and type commands manually:
```bash
wifi_set MySSID MyPass
set_tg_token 12345:Token
set_api_key AIzaKey
restart
```

### 3. Usage

**Chat via Web Interface (New!)**:
1. Open Serial Monitor to see the IP Address (e.g., `192.168.1.50`).
2. Open `http://<IP_ADDRESS>` in your browser/phone.
3. Chat with MicroClaw directly!

**Chat via Telegram (Optional)**:
- Send "Hello!" -> AI replies.
- Send "Open the claw" -> AI calls the `claw_control` tool -> Claw opens -> AI confirms "Claw opened".
- Send "Remember that I like red balls" -> AI writes to `MEMORY.md`.

**Chat via Serial**:
- You can also type directly into the Serial Monitor to interact with the agent (mainly for debugging).

## Development

- **Build**: `cd firmware && pio run`
- **Upload**: `cd firmware && pio run -t upload`
- **Monitor**: `python tools/microclaw.py monitor` or `pio device monitor`

## License

MIT
