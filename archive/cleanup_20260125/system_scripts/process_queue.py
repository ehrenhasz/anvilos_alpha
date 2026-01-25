#!/usr/bin/env python3
import json
import subprocess
import os
import time
from datetime import datetime

QUEUE_FILE = "runtime/card_queue.json"

def load_queue():
    if not os.path.exists(QUEUE_FILE):
        return []
    with open(QUEUE_FILE, "r") as f:
        return json.load(f)

def save_queue(queue):
    with open(QUEUE_FILE, "w") as f:
        json.dump(queue, f, indent=2)

def run_card(card):
    print(f">> PROCESSING CARD: {card['id']}")
    cmd = card.get("command")
    if not cmd:
        return True, "No command"
        
    print(f"   EXEC: {cmd}")
    try:
        # Run with a timeout to avoid hanging forever on a bad clone
        proc = subprocess.run(cmd, shell=True, capture_output=True, text=True) # Removed timeout for now, git can be slow
        if proc.returncode == 0:
            print("   SUCCESS")
            return True, proc.stdout.strip()
        else:
            print(f"   FAILURE: {proc.stderr.strip()}")
            return False, proc.stderr.strip()
    except Exception as e:
        print(f"   EXCEPTION: {e}")
        return False, str(e)

def main():
    queue = load_queue()
    processed_count = 0
    
    for card in queue:
        if card.get("status") == "pending":
            # Mark processing
            card["status"] = "processing"
            card["started_at"] = datetime.now().isoformat()
            save_queue(queue)
            
            # Run
            success, result = run_card(card)
            
            # Mark complete/failed
            card["status"] = "complete" if success else "failed"
            card["completed_at"] = datetime.now().isoformat()
            card["result"] = result
            save_queue(queue)
            
            processed_count += 1
            
            # Optional: Sleep briefly to be nice to the filesystem/network?
            # time.sleep(1)

    print(f"Processed {processed_count} cards.")

if __name__ == "__main__":
    main()
