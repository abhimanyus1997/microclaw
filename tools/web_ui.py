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

# Global variables for serial connection
serial_port = None
serial_connection = None
websocket_clients = set()

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
                        await broadcast_log(f"[DEVICE] {line}")
                else:
                    await asyncio.sleep(0.1)
            except Exception as e:
                logger.error(f"Serial read error: {e}")
                await broadcast_log(f"[ERROR] Serial read error: {e}")
                serial_connection = None
        else:
            await asyncio.sleep(1)

@app.on_event("startup")
async def startup_event():
    asyncio.create_task(read_serial())

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

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
