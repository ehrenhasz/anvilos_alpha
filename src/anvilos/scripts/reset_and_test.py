import sqlite3
import os
import time
import subprocess
import uuid
import json
from datetime import datetime

# CONFIG
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DB_PATH = os.path.join(PROJECT_ROOT, "runtime", "hydrogen.db")
SERVICE_NAME = "bigiron"

def run_cmd(cmd):
    subprocess.run(cmd, shell=True, check=False)

def rebuild_db():
    print(f"[-] Nuking {DB_PATH}...")
    if os.path.exists(DB_PATH):
        os.remove(DB_PATH)
    
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    
    print("[-] Applying Schema v2 (With Offloaded Logic)...")
    
    # 1. Base Table
    cursor.execute("""
        CREATE TABLE jobs (
            correlation_id TEXT PRIMARY KEY,
            idempotency_key TEXT NOT NULL,
            priority INTEGER DEFAULT 50,
            cost_center TEXT DEFAULT 'general',
            status TEXT DEFAULT 'PENDING',
            payload TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    """)
    
    # 2. OFFLOADED LOGIC: Strict Deduplication
    cursor.execute("CREATE UNIQUE INDEX idx_idempotency ON jobs(idempotency_key);")
    
    # 3. OFFLOADED LOGIC: Auto-Timestamping Trigger
    cursor.execute("""
        CREATE TRIGGER update_timestamp 
        AFTER UPDATE ON jobs 
        BEGIN 
            UPDATE jobs SET updated_at = CURRENT_TIMESTAMP 
            WHERE correlation_id = OLD.correlation_id; 
        END;
    """)
    
    # 4. Indexes for speed
    cursor.execute("CREATE INDEX idx_status_prio ON jobs(status, priority DESC);")
    
    conn.commit()
    conn.close()

def inject_test_deck():
    print("[-] Injecting Test Deck...")
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    
    deck = [
        # (Priority, Cost, Instruction, Desc)
        (99, "OPS", "SLEEP", "Critical Sys Update"),
        (10, "MKT", "SLEEP", "Low Prio Blog Post"),
        (50, "DEV", "FAIL", "Bad Code Gen Request"),
        (80, "SEC", "SLEEP", "Security Audit"),
        (99, "OPS", "CRASH", "Memory Overflow Test"),
        (50, "HR", "SLEEP", "Payroll Batch"),
    ]
    
    for prio, cost, instr, desc in deck:
        c_id = str(uuid.uuid4())
        payload = json.dumps({"instruction": instr, "description": desc, "details": desc})
        
        try:
            cursor.execute("""
                INSERT INTO jobs (correlation_id, idempotency_key, priority, cost_center, payload)
                VALUES (?, ?, ?, ?, ?)
            """, (c_id, f"test-{c_id}", prio, cost, payload))
        except Exception as e:
            print(f"[!] Injection Error: {e}")
            
    conn.commit()
    conn.close()
    print(f"[+] Injected {len(deck)} cards.")

def main():
    print("!!! INITIATING SYSTEM RESET !!!")
    run_cmd(f"sudo systemctl stop {SERVICE_NAME}")
    rebuild_db()
    inject_test_deck()
    run_cmd(f"sudo systemctl start {SERVICE_NAME}")
    print("5. DONE. Check your Dashboard!")

if __name__ == "__main__":
    main()
