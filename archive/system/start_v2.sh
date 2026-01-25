#!/bin/bash
# system/start_v2.sh - Persistently start the Gemini Interface V2 and Interceptor Bridge.

PROJECT_ROOT="/mnt/anvil_temp/dnd_vm"
LOG_DIR="$PROJECT_ROOT/logs"
mkdir -p "$LOG_DIR"

# --- Interpretation Layer: Load API Key ---
TOKEN_PATH="$PROJECT_ROOT/config/token"
if [[ -f "$TOKEN_PATH" ]]; then
  export GEMINI_API_KEY=$(cat "$TOKEN_PATH")
  echo "[*] Loaded GEMINI_API_KEY from $TOKEN_PATH"
else
  echo "[!] WARNING: $TOKEN_PATH not found. GEMINI_API_KEY not set."
fi
# --- End Interpretation Layer ---

echo "[*] Starting Interceptor Bridge (Port 8001)..."
pkill -f interceptor_bridge.py
nohup "$PROJECT_ROOT/runtime/venv/bin/python3" "$PROJECT_ROOT/runtime/services/interceptor_bridge.py" > "$LOG_DIR/interceptor_bridge.log" 2>&1 &

echo "[*] Starting Dashboard Server (Port 8000)..."
pkill -f dashboard_server.py
nohup "$PROJECT_ROOT/runtime/venv/bin/python3" "$PROJECT_ROOT/runtime/dashboard_server.py" > "$LOG_DIR/dashboard_server.log" 2>&1 &

echo "[*] Starting Interface V2 Frontend (Port 3001)..."
# We need to ensure we are in the right dir for npm
cd "$PROJECT_ROOT/runtime/interface_v2" || exit 1
# pkill -f "vite --port 3001" # More specific pkill
pkill -f "vite" # Caution: might kill other dashboards

# Start Vite on port 3001
nohup npm run dev -- --port 3001 --host > "$LOG_DIR/interface_v2.log" 2>&1 &

echo "[+] Interface V2 Services Started."
echo "[+] Backend Log: $LOG_DIR/interceptor_bridge.log"
echo "[+] Dashboard Server Log: $LOG_DIR/dashboard_server.log"
echo "[+] Frontend Log: $LOG_DIR/interface_v2.log"
