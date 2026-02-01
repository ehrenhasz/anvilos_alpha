#!/bin/bash
set -e

echo ">> [AUTO-INGEST] Starting mass ingestion loop..."

while true; do
    OUTPUT=$(python3 tools/ingest_staging.py)
    echo "$OUTPUT"
    
    if echo "$OUTPUT" | grep -q "All files processed"; then
        echo ">> [AUTO-INGEST] Job Complete."
        break
    fi
    
    # Optional: minimal sleep to let I/O breathe
    sleep 0.1
done
