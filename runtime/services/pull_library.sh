#!/bin/bash
TARGET_DIR="$HOME/Documents/Ingest_Staging"
LOG_FILE="$HOME/github/dnd_dm/pull_library.log"

mkdir -p "$TARGET_DIR"

echo "[$(date)] Starting Transfer..." > "$LOG_FILE"

sshpass -p '6dD69Kio9S9o' ssh -o StrictHostKeyChecking=no ehren@192.168.6.50 "tar -cf - -C H:/ DriveThruRPG" \
    | tar -xvf - -C "$TARGET_DIR" >> "$LOG_FILE" 2>&1

if [ $? -eq 0 ]; then
    echo "[$(date)] Transfer Complete." >> "$LOG_FILE"
else
    echo "[$(date)] Transfer Failed." >> "$LOG_FILE"
fi
