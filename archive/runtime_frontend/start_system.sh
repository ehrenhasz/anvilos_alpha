#!/bin/bash
# AnvilOS Dashboard & Interceptor Startup Script

PROJECT_ROOT="/mnt/anvil_temp/dnd_vm"
PYTHON_EXEC="/usr/bin/python3"
LOG_DIR="$PROJECT_ROOT/logs"

mkdir -p "$LOG_DIR"

# 1. Start Interceptor Bridge (Port 8001)
echo "Starting Interceptor Bridge..."
nohup $PYTHON_EXEC "$PROJECT_ROOT/runtime/services/interceptor_bridge.py" > "$LOG_DIR/interceptor_bridge.log" 2>&1 &
INTERCEPTOR_PID=$!
echo "Interceptor PID: $INTERCEPTOR_PID"

# 2. Start Dashboard Server (Port 8000)
# This serves the UI and API
echo "Starting Dashboard Server..."
cd "$PROJECT_ROOT/runtime"
# Using exec to replace the shell so systemd tracks this process
exec $PYTHON_EXEC dashboard_server.py > "$LOG_DIR/dashboard_server.log" 2>&1
