#!/bin/bash
# Import Pipeline Cron Wrapper

PROJECT_ROOT="${PROJECT_ROOT:-/mnt/anvil_temp/dnd_vm}"
PYTHON_EXEC="${PYTHON_EXEC:-$PROJECT_ROOT/.venv/bin/python3}"
SCRIPT_PATH="$PROJECT_ROOT/runtime/dnd_imports.py"
LOG_FILE="/var/log/dnd_import.log"

if [ ! -f "$PYTHON_EXEC" ]; then
    PYTHON_EXEC="python3"
fi

if [ ! -w "/var/log" ]; then
    LOG_FILE="$PROJECT_ROOT/logs/import.log"
    mkdir -p "$PROJECT_ROOT/logs"
fi

# Use flock to prevent multiple instances running at once
(
  flock -n 9 || exit 1
  echo "[$(date)] Starting Import Run..." >> "$LOG_FILE"
  cd "$PROJECT_ROOT"
  $PYTHON_EXEC "$SCRIPT_PATH" >> "$LOG_FILE" 2>&1
  echo "[$(date)] Import Run Finished" >> "$LOG_FILE"
) 9>/tmp/dnd_import.lock
