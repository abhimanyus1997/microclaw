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
- **⚙️ CLI Configuration**: runtime configuration for Wi-Fi and API keys via Serial Monitor — no recompiling needed.
- **🔌 Cheap & Accessible**: Runs on a standard ESP32 ($5) with a simple servo ($2).

## Hardware Required

- **ESP32 Development Board** (e.g., ESP32 DevKit V1)
- **Servo Motor** (SG90 or MG996R)
  - Signal Wire -> GPIO 18
  - VCC -> 5V (or external power)
  - GND -> GND
- **Power Supply** (USB or Battery)

## Quick Start

### 1. Installation

**Option A: PlatformIO (Recommended)**
1. Clone this repository.
2. Open the `firmware` folder in VS Code with the PlatformIO extension installed.
3. Click the **Upload** button to flash the firmware.
4. Click **Monitor** to open the Serial Console.

**Option B: Arduino IDE**
1. Install the ESP32 Board support.
2. Install libraries: `ArduinoJson`, `ESP32Servo`.
3. Open `microclaw_esp32/microclaw_esp32.ino`.
4. Flash to your board.

### 2. Configuration (CLI)

MicroClaw uses a Serial Command Line Interface (CLI) for configuration. You don't need to edit code to set passwords!

1. Open the Serial Monitor (Baud Rate: 115200).
2. Enter the following commands:

```bash
# Set Wi-Fi Credentials
wifi_set MySSID MyPassword

# Set Telegram Bot Token (from @BotFather)
set_tg_token 123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11

# Set Gemini API Key (from aistudio.google.com)
set_api_key AIzaSyD...
```

3. Type `restart` to reboot and apply settings.

### 3. Usage

**Chat via Telegram**:
- Send "Hello!" -> AI replies.
- Send "Open the claw" -> AI calls the `claw_control` tool -> Claw opens -> AI confirms "Claw opened".
- Send "Remember that I like red balls" -> AI writes to `MEMORY.md`.

**Chat via Serial**:
- You can also type directly into the Serial Monitor to interact with the agent (mainly for debugging).

## Architecture

- **`microclaw_esp32.ino`**: Main non-blocking loop handling CLI, Telegram polling, and Agent logic.
- **`gemini_client.h`**: HTTPS client for Google Gemini API.
- **`telegram_bot.h`**: HTTPS client for Telegram Bot API (Long Polling implementation).
- **`tools.h`**: Implementation of agent tools (`claw_control`, `memory_read`, `memory_write`).
- **`file_system.h`**: Wrapper for LittleFS to store config and memory.
- **`cli.h`**: Serial input parser for runtime configuration.

## Roadmap

- [x] Basic Control
- [x] Gemini Integration
- [x] Telegram Bot
- [x] Data Persistence
- [ ] Voice Control (Audio Support)
- [ ] Vision Capabilities (ESP32-CAM)

## License

MIT
