#!/bin/bash
# PROTOCOL BELLOWS: THE AUTOMATION DAEMON
# WATCHES: /mnt/expanded_memory/anvil_jobs/inbox
# TRIGGERS: forge_iso.sh

INBOX="/mnt/expanded_memory/anvil_jobs/inbox"
OUTBOX="/mnt/expanded_memory/anvil_jobs/outbox"
LOGS="/mnt/expanded_memory/anvil_jobs/logs"
ANVIL_ROOT="/mnt/expanded_memory/anvil_iso_gold"

echo ">> [BELLOWS] SYSTEMS ONLINE. WATCHING INBOX..."

while true; do
    # Check for any .jcl file in the inbox
    if ls $INBOX/*.jcl 1> /dev/null 2>&1; then
        for JOB_FILE in $INBOX/*.jcl; do
            [ -e "$JOB_FILE" ] || continue
            JOB_ID=$(basename "$JOB_FILE" .jcl)
            TIMESTAMP=$(date +%Y%m%d-%H%M%S)
            LOG_FILE="$LOGS/${JOB_ID}_${TIMESTAMP}.log"
            
            echo ">> [BELLOWS] JOB DETECTED: $JOB_ID"
            echo ">> [BELLOWS] EXTRACTING PAYLOAD..."
            
            # EXTRACT PAYLOAD
            sed -n '/>>> PAYLOAD_START/,/>>> PAYLOAD_END/p' "$JOB_FILE" | grep -v '>>> PAYLOAD' > "$ANVIL_ROOT/build/rootfs/boot/genesis.py"

            echo ">> [BELLOWS] IGNITING FORGE..."
            
            # MOVE JOB TO PROCESSING (To prevent double-reads)
            mv "$JOB_FILE" "$INBOX/${JOB_ID}.processing"
            
            # EXECUTE THE FORGE
            (
                cd $ANVIL_ROOT
                ./forge_iso.sh
            ) > "$LOG_FILE" 2>&1
            
            # CHECK STATUS
            if [ $? -eq 0 ]; then
                echo ">> [BELLOWS] JOB COMPLETE: SUCCESS."
                # Move the ISO to the outbox with a unique name
                mv "$ANVIL_ROOT/artifacts/anvil_universal.iso" "$OUTBOX/anvil_${JOB_ID}.iso"
                rm "$INBOX/${JOB_ID}.processing"
            else
                echo "!! [BELLOWS] JOB FAILED. CHECK LOGS."
                mv "$INBOX/${JOB_ID}.processing" "$INBOX/${JOB_ID}.failed"
            fi
        done
    fi
    sleep 2
done
