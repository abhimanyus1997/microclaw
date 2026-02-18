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
        "--before", "default_reset",
        "--after", "hard_reset",
        "write_flash", "-z",
        "--flash_mode", "dio",
        "--flash_freq", "40m",
        "--flash_size", "detect",
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
        
        flash_cmd = [
            sys.executable, "-m", "esptool",
            "--chip", "esp32",
            "--port", port,
            "--baud", "460800",
            "--before", "default_reset",
            "--after", "hard_reset",
            "write_flash", "-z",
            "--flash_mode", "dio",
            "--flash_size", "detect",
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

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
