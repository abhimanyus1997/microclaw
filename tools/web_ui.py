from typing import Optional
from fastapi import FastAPI, WebSocket, Request, BackgroundTasks
from fastapi.templating import Jinja2Templates
from fastapi.staticfiles import StaticFiles
from fastapi.responses import JSONResponse
import serial
import serial.tools.list_ports
import asyncio
import json
import logging
import logging
import os
import shutil
import sys
from fastapi import FastAPI, WebSocket, Request, BackgroundTasks, UploadFile, Form

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = FastAPI()

# Mount static files and templates
app.mount("/static", StaticFiles(directory="tools/static"), name="static")
templates = Jinja2Templates(directory="tools/templates")

# Global state
serial_port: Optional[serial.Serial] = None
serial_connection: Optional[serial.Serial] = None
connected_port: str = ""
websocket_clients = set()
flashing_in_progress = False

# Helper to broadcast messages to all connected clients
async def broadcast_log(message: str):
    if not websocket_clients:
        return
    disconnected_clients = set()
    for client in websocket_clients:
        try:
            await client.send_text(message)
        except Exception:
            disconnected_clients.add(client)
    for client in disconnected_clients:
        websocket_clients.remove(client)

# Background task to read from serial port
async def read_serial():
    global serial_connection
    while True:
        if serial_connection and serial_connection.is_open:
            try:
                if serial_connection.in_waiting > 0:
                    line = serial_connection.readline().decode('utf-8', errors='replace').strip()
                    if line:
                        # Intercept System Info JSON
                        if line.startswith("{") and "heap_free" in line:
                            # Log as info but don't broadcast to main console?
                            # Actually, we broadcast but tag as [SYS_INFO].
                            # The frontend can then decide to hide it from the log view.
                            # BUT the user wants it gone from the dashboard console.
                            # So we broadcast it so the "Health" charts get it, but we
                            # don't broadcast it as a standard [DEVICE] log.
                            await broadcast_log(f"[SYS_INFO] {line}")
                        elif line.startswith("{") and "\"thought\"" in line:
                            # AI Thought block
                            await broadcast_log(f"[AI_JSON] {line}")
                        else:
                            await broadcast_log(f"[DEVICE] {line}")
                else:
                    await asyncio.sleep(0.1)
            except Exception as e:
                logger.error(f"Serial read error: {e}")
                await broadcast_log(f"[ERROR] Serial read error: {e}")
                serial_connection = None
        else:
            await asyncio.sleep(1)

# Background task to poll system info
async def poll_system_info():
    while True:
        if serial_connection and serial_connection.is_open and not flashing_in_progress:
            try:
                # Send system_info command silently
                serial_connection.write(b"system_info\n")
            except Exception as e:
                logger.error(f"Poll error: {e}")
        await asyncio.sleep(5) # Poll every 5 seconds

@app.on_event("startup")
async def startup_event():
    asyncio.create_task(read_serial())
    asyncio.create_task(poll_system_info())

@app.get("/")
async def get_index(request: Request):
    return templates.TemplateResponse("index.html", {"request": request})

@app.get("/api/ports")
async def get_ports():
    # Smart filtering for ESP32/USB-Serial devices
    all_ports = serial.tools.list_ports.comports()
    ports = []
    for p in all_ports:
        # Check device path or description for likely candidates
        if "USB" in p.device.upper() or "ACM" in p.device.upper() or "USB" in p.description.upper():
            ports.append(p.device)
            
    # Fallback: If no specific ports found, return all (user might have non-standard setup)
    if not ports and all_ports:
        ports = [p.device for p in all_ports]
        
    return {"ports": sorted(ports)}

@app.post("/api/connect")
async def connect_port(request: Request):
    global serial_connection, serial_port
    data = await request.json()
    port = data.get("port")
    
    if not port:
        return JSONResponse(status_code=400, content={"error": "Port required"})

    try:
        if serial_connection and serial_connection.is_open:
            serial_connection.close()
        
        serial_connection = serial.Serial(port, 115200, timeout=1)
        serial_port = port
        await broadcast_log(f"[SYSTEM] Connected to {port}")
        return {"status": "connected", "port": port}
    except Exception as e:
        return JSONResponse(status_code=500, content={"error": str(e)})

@app.post("/api/disconnect")
async def disconnect_port():
    global serial_connection, serial_port
    if serial_connection and serial_connection.is_open:
        serial_connection.close()
    serial_connection = None
    serial_port = None
    await broadcast_log("[SYSTEM] Disconnected")
    return {"status": "disconnected"}

@app.post("/api/command")
async def send_command(request: Request):
    global serial_connection
    if not serial_connection or not serial_connection.is_open:
        return JSONResponse(status_code=400, content={"error": "Not connected"})
    
    data = await request.json()
    command = data.get("command")
    
    if command:
        try:
            full_cmd = f"{command}\n"
            serial_connection.write(full_cmd.encode())
            await broadcast_log(f"[SENT] {command}")
            return {"status": "sent", "command": command}
        except Exception as e:
            return JSONResponse(status_code=500, content={"error": str(e)})
    return {"status": "ignored"}

async def sync_config_to_device(port, config_data):
    """Helper to push config.json values to device via Serial"""
    global serial_connection
    
    close_after = False
    if not serial_connection or not serial_connection.is_open:
        try:
            serial_connection = serial.Serial(port, 115200, timeout=1)
            close_after = True
        except Exception as e:
            await broadcast_log(f"[ERROR] Failed to open port for sync: {e}")
            return False

    try:
        commands = []
        # Push keys first
        if config_data.get("gemini_key"):
            commands.append(f'set_api_key "{config_data["gemini_key"]}"')
        if config_data.get("groq_key"):
            commands.append(f'set_groq_key "{config_data["groq_key"]}"')
        if config_data.get("assistant_provider"):
            commands.append(f'set_provider "{config_data.get("assistant_provider")}"')
        if config_data.get("telegram_token"):
            commands.append(f'set_tg_token "{config_data.get("telegram_token")}"')
            
        # Push WiFi last as it triggers a reboot in current firmware
        if config_data.get("wifi_ssid"):
            ssid = config_data["wifi_ssid"]
            pwd = config_data.get("wifi_password", "")
            commands.append(f'wifi_set "{ssid}" "{pwd}"')

        for cmd in commands:
            await broadcast_log(f"[SYSTEM] Syncing: {cmd}")
            serial_connection.write(f"{cmd}\n".encode())
            await asyncio.sleep(0.8) # Wait for device to process
            
        return True
    except Exception as e:
        await broadcast_log(f"[ERROR] Sync failed: {e}")
        return False
    finally:
        if close_after and serial_connection:
            serial_connection.close()
            serial_connection = None

@app.post("/api/sync_config")
async def api_sync_config(request: Request):
    global serial_port
    data = await request.json()
    port = data.get("port") or serial_port
    
    if not port:
        return JSONResponse(status_code=400, content={"error": "Port required"})
        
    config_data = _load_config()
    if not config_data:
        return JSONResponse(status_code=400, content={"error": "Local config.json not found"})
        
    success = await sync_config_to_device(port, config_data)
    if success:
        return {"status": "success"}
    else:
        return JSONResponse(status_code=500, content={"error": "Sync failed"})

@app.websocket("/ws/logs")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    websocket_clients.add(websocket)
    try:
        while True:
            # We don't expect much input from client via WS, mostly output
            # But we must keep the connection alive
            await websocket.receive_text()
    except Exception:
        pass
    finally:
        websocket_clients.remove(websocket)

@app.post("/api/flash")
async def flash_firmware(file: UploadFile, port: str = Form(...)):
    global serial_connection
    
    if not port:
        return JSONResponse(status_code=400, content={"error": "Port required"})

    # 1. Save uploaded file
    temp_filename = f"temp_firmware.bin"
    try:
        with open(temp_filename, "wb") as buffer:
            shutil.copyfileobj(file.file, buffer)
    except Exception as e:
        return JSONResponse(status_code=500, content={"error": f"Upload failed: {e}"})

    # 2. Close Serial if open
    was_connected = False
    if serial_connection and serial_connection.is_open:
        await broadcast_log("[SYSTEM] Closing serial for flashing...")
        serial_connection.close()
        serial_connection = None
        was_connected = True

    # 3. Flashing Process
    await broadcast_log(f"[SYSTEM] Starting Flash on {port}...")
    
    # Using esptool args similar to CLI
    cmd = [
        sys.executable, "-m", "esptool",
        "--chip", "esp32",
        "--port", port,
        "--baud", "460800",
        "--before", "default-reset",
        "--after", "hard-reset",
        "write-flash", "-z",
        "--flash-mode", "dio",
        "--flash-freq", "40m",
        "--flash-size", "detect",
        "0x10000", temp_filename
    ]

    try:
        process = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE
        )
        
        # Stream output
        while True:
            line = await process.stdout.readline()
            if not line:
                break
            msg = line.decode().strip()
            if msg:
                await broadcast_log(f"[FLASH] {msg}")

        await process.wait()
        
        if process.returncode == 0:
            await broadcast_log("[SYSTEM] Flashing Complete! Rebooting...")
            return {"status": "success"}
        else:
            stderr = await process.stderr.read()
            err_msg = stderr.decode().strip()
            await broadcast_log(f"[ERROR] Flash Failed: {err_msg}")
            return JSONResponse(status_code=500, content={"error": f"Flash failed: {err_msg}"})
            
    except Exception as e:
        return JSONResponse(status_code=500, content={"error": str(e)})
    finally:
        if os.path.exists(temp_filename):
            os.remove(temp_filename)
        # We don't auto-reconnect because device is rebooting

@app.get("/api/config")
async def get_config():
    config_path = "config.json"
    if os.path.exists(config_path):
        try:
            with open(config_path, "r") as f:
                return json.load(f)
        except Exception as e:
            return JSONResponse(status_code=500, content={"error": str(e)})
    return {}

@app.post("/api/build_flash")
async def build_and_flash(port: str = Form(...)):
    global serial_connection, flashing_in_progress
    
    # Path to where PIO builds the binary
    # We assume running from project root
    firmware_binary = os.path.join("firmware", ".pio", "build", "esp32dev", "firmware.bin")
    
    try:
        flashing_in_progress = True
        
        # 1. Build Firmware
        await broadcast_log(f"[SYSTEM] Building Firmware...")
        
        # Always use module invocation to avoid PATH issues in uv/venvs
        build_cmd = [sys.executable, "-m", "platformio", "run", "-d", "firmware"]
        logger.info(f"Executing Build Command: {build_cmd}")
        
        process_build = await asyncio.create_subprocess_exec(
            *build_cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE
        )
        
        # Stream build output
        async for line in process_build.stdout:
            msg = line.decode().strip()
            if msg:
                # Log only important lines to console to avoid clutter, but all to WS
                if "[ERROR]" in msg or "Error" in msg or "Failed" in msg:
                    logger.error(f"Build Output: {msg}")
                await broadcast_log(f"[BUILD] {msg}")
            
        await process_build.wait()
        
        if process_build.returncode != 0:
            stderr = await process_build.stderr.read()
            err_msg = stderr.decode().strip()
            logger.error(f"Build Failed Code {process_build.returncode}: {err_msg}")
            await broadcast_log(f"[ERROR] Build Failed: {err_msg}")
            flashing_in_progress = False
            return JSONResponse(status_code=500, content={"error": f"Build failed: {err_msg}"})

        await broadcast_log("[SYSTEM] Build Success! Starting Flash...")
        logger.info("Build Successful. Proceeding to Flash.")

        # 2. Close Serial if open
        if serial_connection and serial_connection.is_open:
            await broadcast_log("[SYSTEM] Closing serial for flashing...")
            serial_connection.close()
            serial_connection = None

        # 3. Flash Firmware
        await broadcast_log(f"[SYSTEM] Flashing to {port}...")
        
        # Define paths for all binaries
        PARTITIONS_BIN = os.path.join("firmware", ".pio", "build", "esp32dev", "partitions.bin")
        BOOTLOADER_BIN = os.path.join("firmware", ".pio", "build", "esp32dev", "bootloader.bin")

        flash_cmd = [
            sys.executable, "-m", "esptool",
            "--chip", "esp32",
            "--port", port,
            "--baud", "460800",
            "--before", "default-reset",
            "--after", "hard-reset",
            "write-flash", "-z",
            "--flash-mode", "dio",
            "--flash-size", "detect",
            "0x1000", BOOTLOADER_BIN,
            "0x8000", PARTITIONS_BIN,
            "0x10000", firmware_binary
        ]

        process_flash = await asyncio.create_subprocess_exec(
            *flash_cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE
        )
        
        while True:
            line = await process_flash.stdout.readline()
            if not line: break
            msg = line.decode().strip()
            if msg: await broadcast_log(f"[FLASH] {msg}")

        await process_flash.wait()
        
        if process_flash.returncode == 0:
            await broadcast_log("[SYSTEM] Build & Flash Complete! Rebooting...")
            
            # Proactive: Sync config after reboot
            config_data = _load_config()
            if config_data:
                await broadcast_log("[SYSTEM] Waiting 5s for device to boot before syncing config...")
                await asyncio.sleep(5)
                await sync_config_to_device(port, config_data)
                
            return {"status": "success"}
        else:
            stderr = await process_flash.stderr.read()
            err_msg = stderr.decode().strip()
            await broadcast_log(f"[ERROR] Flash Failed: {err_msg}")
            return JSONResponse(status_code=500, content={"error": f"Flash failed: {err_msg}"})
            
    except Exception as e:
        logger.error(f"Build/Flash Error: {e}")
        await broadcast_log(f"[ERROR] Exception: {str(e)}")
        return JSONResponse(status_code=500, content={"error": str(e)})
    finally:
        flashing_in_progress = False

# ── AI Assistant ──────────────────────────────────────────────────────────────
import urllib.request
import urllib.error

ASSISTANT_SYSTEM_PROMPT = """You are MicroClaw Assistant — a helpful AI built into the MicroClaw Manager desktop application.

MicroClaw is an ESP32-based Physical AI Agent that uses Google Gemini or Groq LLMs to interact with hardware.
Key knowledge:
- Config file (config.json): wifi_ssid, wifi_password, gemini_key, gemini_model, groq_key, groq_model
- Firmware is built with PlatformIO (pio run -d firmware) and flashed via esptool
- The ESP32 runs a web UI at its IP for AI chat, and exposes tools: wifi_scan, ble_scan, gpio_set, gpio_read, memory_write, memory_read
- **NEW**: `run_script` tool for running JSON-based hardware scripts (gpio, delay, loops). Example: `{"cmd": "loop", "count": 5, "steps": [...]}`

- The Manager (this app) monitors serial output, configures WiFi/API keys, and can build+flash firmware
- Quick start: plug in ESP32 → connect in Manager → set WiFi + API key → flash → device reboots and joins WiFi

You can answer any question — about MicroClaw setup, troubleshooting, general programming, or anything else.
Be concise but thorough. Format responses with markdown when helpful."""

assistant_history = []  # In-memory chat history for context

def _load_config():
    """Load config.json"""
    config_path = "config.json"
    if os.path.exists(config_path):
        with open(config_path, "r") as f:
            return json.load(f)
    return {}

def _save_config(config):
    """Save config.json"""
    with open("config.json", "w") as f:
        json.dump(config, f, indent=4)

from curl_cffi import requests

def _call_gemini(api_key: str, model: str, messages: list) -> str:
    """Call Gemini API via curl_cffi (impersonating browser to avoid 403)"""
    url = f"https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent?key={api_key}"
    
    # Build Gemini contents format
    contents = []
    for msg in messages:
        role = "user" if msg["role"] == "user" else "model"
        contents.append({"role": role, "parts": [{"text": msg["content"]}]})
    
    payload = {
        "contents": contents,
        "systemInstruction": {"parts": [{"text": ASSISTANT_SYSTEM_PROMPT}]},
        "generationConfig": {"temperature": 0.7, "maxOutputTokens": 2048}
    }
    
    try:
        resp = requests.post(
            url, 
            json=payload, 
            headers={"Content-Type": "application/json"},
            impersonate="chrome"
        )
        resp.raise_for_status()
        data = resp.json()
        return data["candidates"][0]["content"]["parts"][0]["text"]
    except Exception as e:
        logger.error(f"Gemini API Error: {e}")
        if hasattr(e, 'response') and e.response:
            logger.error(f"Response: {e.response.text}")
        raise

def _call_groq(api_key: str, model: str, messages: list) -> str:
    """Call Groq API via curl_cffi (impersonating browser)"""
    url = "https://api.groq.com/openai/v1/chat/completions"
    
    groq_messages = [{"role": "system", "content": ASSISTANT_SYSTEM_PROMPT}]
    for msg in messages:
        groq_messages.append({"role": msg["role"], "content": msg["content"]})
    
    payload = {
        "model": model,
        "messages": groq_messages,
        "temperature": 0.7,
        "max_tokens": 2048
    }
    
    try:
        resp = requests.post(
            url, 
            json=payload, 
            headers={
                "Content-Type": "application/json",
                "Authorization": f"Bearer {api_key}"
            },
            impersonate="chrome"
        )
        resp.raise_for_status()
        data = resp.json()
        return data["choices"][0]["message"]["content"]
    except Exception as e:
        logger.error(f"Groq API Error: {e}")
        if hasattr(e, 'response') and e.response:
            logger.error(f"Response: {e.response.text}")
        raise

@app.post("/api/assistant")
async def assistant_chat(request: Request):
    global assistant_history
    config = _load_config()
    
    # Check if assistant is enabled
    if not config.get("assistant_enabled", True):
        return JSONResponse(status_code=403, content={"error": "Assistant is disabled"})
    
    data = await request.json()
    user_msg = data.get("message", "").strip()
    if not user_msg:
        return JSONResponse(status_code=400, content={"error": "Empty message"})
    
    # Add user message to history
    assistant_history.append({"role": "user", "content": user_msg})
    
    # Keep history manageable (last 20 messages)
    if len(assistant_history) > 20:
        assistant_history = assistant_history[-20:]
    
    # Determine provider
    provider = config.get("assistant_provider", "gemini")
    
    try:
        if provider == "groq":
            api_key = config.get("groq_key", "")
            model = config.get("groq_model", "llama-3.3-70b-versatile")
            if not api_key:
                return JSONResponse(status_code=400, content={"error": "Groq API key not set in config"})
            reply = await asyncio.to_thread(_call_groq, api_key, model, assistant_history)
        else:
            api_key = config.get("gemini_key", "")
            model = config.get("gemini_model", "gemini-2.5-flash")
            if not api_key:
                return JSONResponse(status_code=400, content={"error": "Gemini API key not set in config"})
            reply = await asyncio.to_thread(_call_gemini, api_key, model, assistant_history)
        
        # Add assistant reply to history
        assistant_history.append({"role": "assistant", "content": reply})
        return {"reply": reply, "provider": provider}
    
    except urllib.error.HTTPError as e:
        err_body = e.read().decode() if e.fp else str(e)
        logger.error(f"Assistant API error: {err_body}")
        return JSONResponse(status_code=502, content={"error": f"API error ({e.code}): {err_body[:200]}"})
    except Exception as e:
        logger.error(f"Assistant error: {e}")
        return JSONResponse(status_code=500, content={"error": str(e)})

@app.get("/api/assistant/config")
async def get_assistant_config():
    config = _load_config()
    return {
        "enabled": config.get("assistant_enabled", True),
        "provider": config.get("assistant_provider", "gemini")
    }

@app.post("/api/assistant/config")
async def set_assistant_config(request: Request):
    global assistant_history
    data = await request.json()
    config = _load_config()
    
    if "enabled" in data:
        config["assistant_enabled"] = bool(data["enabled"])
    if "provider" in data and data["provider"] in ("gemini", "groq"):
        config["assistant_provider"] = data["provider"]
        # Clear history on provider switch
        assistant_history = []
    
    _save_config(config)
    return {"status": "ok", "enabled": config.get("assistant_enabled", True), "provider": config.get("assistant_provider", "gemini")}

@app.post("/api/assistant/clear")
async def clear_assistant_history():
    global assistant_history
    assistant_history = []
    return {"status": "cleared"}

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
