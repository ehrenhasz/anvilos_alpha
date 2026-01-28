#!/usr/bin/env python3
import os
import sys
import time
import sqlite3
import json
import uuid
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.append(PROJECT_ROOT)
DB_PATH = os.path.join(PROJECT_ROOT, "data", "cortex.db")
try:
    from forge_directives import PHASE2_DIRECTIVES
except ImportError:
    print("FATAL: forge_directives.py not found.")
    sys.exit(1)
BATCH_SIZE = 500
def get_stack_count(cursor):
    cursor.execute("SELECT COUNT(*) FROM card_stack WHERE stat = 0")
    return cursor.fetchone()[0]

def main():
    print(f"[*] Feeding The Forge (Direct Injection). Total Directives: {len(PHASE2_DIRECTIVES)}")
    print(f"[*] Batch Size: {BATCH_SIZE}")
    
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    
    total_injected = 0
    
    for i in range(0, len(PHASE2_DIRECTIVES), BATCH_SIZE):
        batch = PHASE2_DIRECTIVES[i : i + BATCH_SIZE]
        print(f"\n[>] Injecting Batch {i // BATCH_SIZE + 1} ({len(batch)} cards)...")
        
        for item in batch:
            card_id = str(uuid.uuid4())
            seq = item.get("seq", 999)
            op = item.get("op", "unknown")
            pld = item.get("pld", {})
            
            # Mark source as trusted
            pld["_source"] = "COMMANDER"
            
            # Insert directly into card_stack
            cursor.execute(
                "INSERT INTO card_stack (id, seq, op, pld, stat, timestamp) VALUES (?, ?, ?, ?, ?, ?)",
                (card_id, seq, op, json.dumps(pld), 0, time.time())
            )
            
        conn.commit()
        total_injected += len(batch)
        print(f"[+] Total Injected: {total_injected}")
        
        print("[*] Waiting for Processor to consume...")
        while True:
            pending = get_stack_count(cursor)
            if pending < 100: # Keep buffer low to avoid DB lock contention? 
                break
            sys.stdout.write(f"\r... Stack Depth: {pending}   ")
            sys.stdout.flush()
            time.sleep(2)
            
    print("\n[!] All directives injected directly.")
if __name__ == "__main__":
    main()
