#!/bin/bash

# MicroClaw Manager - Linux Runner
# Automatically handles uv installation, dependency syncing, and admin escalation.

set -e

echo "--- MicroClaw Manager Launcher ---"

# 1. Check for uv
if ! command -v uv &> /dev/null; then
    echo "uv not found. Installing uv..."
    curl -LsSf https://astral.sh/uv/install.sh | sh
    # Add to path for current session
    export PATH="$HOME/.cargo/bin:$PATH"
fi

# 2. Sync dependencies
echo "Syncing dependencies..."
uv sync

# 3. Inform user about sudo
echo "Launching main.py..."
echo "Note: Sudo password may be required for serial port access."

# 4. Run main.py
# Use -E to preserve environment (like PATH for uv)
sudo -E uv run python main.py
