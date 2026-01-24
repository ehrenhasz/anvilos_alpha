#!/bin/bash
TARGET_DIR="$HOME/Documents/Ingest_Staging"
LIST_FILE="remote_folders.txt"
LOG_FILE="robust_pull.log"

mkdir -p "$TARGET_DIR"

echo "[$(date)] Starting Robust Pull..." > "$LOG_FILE"

while IFS= read -r folder; do
    # Trim Windows CR chars if any
    folder=$(echo "$folder" | tr -d '\r')
    
    if [ -z "$folder" ]; then continue; fi
    
    if [ -d "$TARGET_DIR/$folder" ]; then
        echo "Skipping '$folder' (Exists)" >> "$LOG_FILE"
    else
        echo "Downloading '$folder'..." >> "$LOG_FILE"
        # Using scp with quotes for remote path
        sshpass -p '6dD69Kio9S9o' scp -r -o StrictHostKeyChecking=no \
            ehren@192.168.6.50:"H:/DriveThruRPG/$folder" "$TARGET_DIR/" >> "$LOG_FILE" 2>&1
        
        if [ $? -eq 0 ]; then
            echo "Success: '$folder'" >> "$LOG_FILE"
        else
            echo "Failed: '$folder'" >> "$LOG_FILE"
        fi
    fi
done < "$LIST_FILE"

echo "[$(date)] Pull Process Complete." >> "$LOG_FILE"

