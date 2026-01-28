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
def get_pending_count(cursor):
    cursor.execute("SELECT COUNT(*) FROM sys_goals WHERE stat IN (0, 1)")
    return cursor.fetchone()[0]
def main():
    print(f"[*] Feeding The Forge. Total Directives: {len(PHASE2_DIRECTIVES)}")
    print(f"[*] Batch Size: {BATCH_SIZE}")
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    total_injected = 0
    for i in range(0, len(PHASE2_DIRECTIVES), BATCH_SIZE):
        batch = PHASE2_DIRECTIVES[i : i + BATCH_SIZE]
        print(f"\n[>] Injecting Batch {i // BATCH_SIZE + 1} ({len(batch)} cards)...")
        for card in batch:
            goal_id = str(uuid.uuid4())
            seq = card.get("seq", 999)
            op = card.get("op", "unknown")
            pld = card.get("pld", {})
            goal_text = f"Directive {seq}: {op}. Payload: {json.dumps(pld)}"
            cursor.execute("INSERT INTO sys_goals (goal, stat, timestamp) VALUES (?, 0, ?)", 
                           (goal_text, time.time()))
        conn.commit()
        total_injected += len(batch)
        print(f"[+] Total Injected: {total_injected}")
        print("[*] Waiting for Forge to consume...")
        while True:
            pending = get_pending_count(cursor)
            if pending < 50: # Keep a buffer of 50
                break
            sys.stdout.write(f"\r... Pending Goals: {pending}   ")
            sys.stdout.flush()
            time.sleep(2)
    print("\n[!] All directives injected.")
if __name__ == "__main__":
    main()
