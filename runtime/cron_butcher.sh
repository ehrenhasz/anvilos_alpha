#!/bin/bash
# Butcher Cron Wrapper

PROJECT_ROOT="${PROJECT_ROOT:-/mnt/anvil_temp/dnd_vm}"
PYTHON_EXEC="${PYTHON_EXEC:-$PROJECT_ROOT/.venv/bin/python3}"
SCRIPT_PATH="$PROJECT_ROOT/runtime/the_butcher.py"
LOG_FILE="/var/log/dnd_butcher.log"

# Fallback to system python if venv not found
if [ ! -f "$PYTHON_EXEC" ]; then
    PYTHON_EXEC="python3"
fi

# Ensure log file exists and is writable (requires sudo usually, but we'll try)
# If running as user, maybe map to local log dir? User asked for /var/log
if [ ! -w "/var/log" ]; then
    LOG_FILE="$PROJECT_ROOT/logs/butcher.log"
    mkdir -p "$PROJECT_ROOT/logs"
fi

# Run the butcher
echo "[$(date)] Starting Butcher Run..." >> "$LOG_FILE"
cd "$PROJECT_ROOT"
$PYTHON_EXEC "$SCRIPT_PATH" >> "$LOG_FILE" 2>&1
EXIT_CODE=$?
echo "[$(date)] Butcher Run Finished with Exit Code $EXIT_CODE" >> "$LOG_FILE"
