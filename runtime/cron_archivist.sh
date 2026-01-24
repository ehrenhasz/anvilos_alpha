#!/bin/bash
# Archivist Cron Wrapper

PROJECT_ROOT="${PROJECT_ROOT:-/mnt/anvil_temp/dnd_vm}"
PYTHON_EXEC="${PYTHON_EXEC:-$PROJECT_ROOT/.venv/bin/python3}"
SCRIPT_PATH="$PROJECT_ROOT/runtime/library_archivist.py"
LOG_FILE="/var/log/dnd_archivist.log"

if [ ! -f "$PYTHON_EXEC" ]; then
    PYTHON_EXEC="python3"
fi

if [ ! -w "/var/log" ]; then
    LOG_FILE="$PROJECT_ROOT/logs/archivist.log"
    mkdir -p "$PROJECT_ROOT/logs"
fi

# Use flock to prevent multiple instances
(
  flock -n 9 || exit 1
  echo "[$(date)] Starting Archivist Run..." >> "$LOG_FILE"
  cd "$PROJECT_ROOT"
  $PYTHON_EXEC "$SCRIPT_PATH" >> "$LOG_FILE" 2>&1
  echo "[$(date)] Archivist Run Finished" >> "$LOG_FILE"
) 9>/tmp/dnd_archivist.lock
