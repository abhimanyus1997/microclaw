@echo off
setlocal

echo --- MicroClaw Manager Launcher ---

:: 1. Check for uv
where uv >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo uv not found. Installing uv...
    powershell -ExecutionPolicy ByPass -c "irm https://astral.sh/uv/install.ps1 | iex"
    :: Add path for current session if possible, though installer usually handles it
    set "PATH=%USERPROFILE%\.cargo\bin;%PATH%"
)

:: 2. Sync dependencies
echo Syncing dependencies...
uv sync

:: 3. Run main.py
echo Launching main.py...
uv run python main.py

pause
