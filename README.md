<div align="center">

# <img src="docs/logo.png" height="40" valign="middle"> MicroClaw 🦞
### AI on Hardware (AoH) for ESP32

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Build-orange.svg)](https://platformio.org/)
[![Python](https://img.shields.io/badge/Python-3.10+-yellow.svg)](https://python.org)
[![Live Docs](https://img.shields.io/badge/Live-Docs-ff6b6b?style=flat&logo=github)](https://abhimanyus1997.github.io/microclaw/)

MicroClaw is NOT just a claw controller. It transforms a \$5 ESP32 into a **general-purpose physical AI agent** capable of reasoning, remembering, and acting in the real world. Powered by **Google Gemini** or **Groq**, it can control ANY hardware connected to the ESP32 (sensors, relays, motors, LEDs) through natural language.

[Features](#features) • [Quick Start](#quick-start) • [Web UI](#web-dashboard) • [Hardware](#hardware)

</div>

---

## ✨ Features

- **🧠 Dual Brain**: Switch between **Google Gemini 1.5 Flash** (Multimodal) and **Groq** (Llama 3, Ultra-Fast).
- **🕸️ MicroClaw Manager**: A professional, dark-themed Web UI for configuration, flashing, and monitoring.
- **🦾 Tool Use**: The AI can autonomously control servos (Open, Close, Wave) and manage its own memory.
- **💾 Long-Term Memory**: Remembers context across reboots using persistent file storage (`MEMORY.md`).
- **💬 Multi-Interface**: Chat via **Web UI**, **Telegram**, or **Serial Terminal**.
- **🔌 Zero-Friction Setup**: Auto-detects ports, interactive CLI wizard, and one-click firmware flashing.

## 🚀 Quick Start

### 1. Installation
Requires Python 3.10+. We recommend using `uv` for speed.

```bash
# Install tool dependencies
uv pip install -r tools/requirements.txt
```

### 2. Launch the Manager
The **MicroClaw Manager** handles everything: permissions, flashing, and chatting.

```bash
# Launches the Web UI (asks for sudo password once for USB access)
python3 main.py
```

Open **[http://localhost:8000](http://localhost:8000)** in your browser.

### 3. Setup & Flash
1.  Go to the **Setup Tab** in the Web UI.
2.  **Flash Firmware**: Upload `firmware.bin` (or build it yourself) and click Flash.
3.  **Configure**: Enter your Wi-Fi credentials and API Keys (Gemini or Groq).
4.  **Connect**: Switch to the **Monitor Tab** to see the AI boot up!

## 🖥️ Web Dashboard

The new **MicroClaw Manager** features a professional, space-themed interface:
- **Real-time Terminal**: Monitor serial logs with color-coded ease.
- **Smart Control**: Restart device, scan Wi-Fis, and send commands.
- **Secure Config**: Set API keys without editing code.

## 🛠️ Hardware

- **ESP32 Development Board** (ESP32-WROOM-32 / DevKit V1)
- **Servo Motor** (SG90 or MG996R)
  - `Signal` -> GPIO 18
  - `VCC` -> 5V / VIN
  - `GND` -> GND
- **Power**: USB Cable or 5V Battery

## 👨‍💻 Development

Everything is open source.

**Firmware (C++)**:
```bash
cd firmware
pio run -t upload
```

**CLI Tool (Python)**:
```bash
python tools/microclaw.py --help
```

---

<div align="center">
Created by <b>Abhimanyu Singh</b> • <a href="https://github.com/abhimanyus1997">GitHub</a> • <a href="https://linkedin.com/in/abhimanyus1997">LinkedIn</a>
</div>

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
