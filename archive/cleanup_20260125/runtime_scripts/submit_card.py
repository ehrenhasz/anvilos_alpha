import sqlite3
import uuid
import json
import os

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DB_PATH = os.path.join(PROJECT_ROOT, "runtime", "hydrogen.db")

def submit_ops_cycle(message: str, priority: int = 50):
    """
    Triggers the full OPS Lifecycle:
    Branch -> Commit -> Push -> PR -> Merge -> Clean.
    """
    if not os.path.exists(DB_PATH):
        return "ERROR: DB Offline"

    c_id = str(uuid.uuid4())
    idem_key = f"ops-{abs(hash(message))}" 
    
    payload = json.dumps({
        "instruction": "OPS_CYCLE", 
        "details": message
    })

    try:
        with sqlite3.connect(DB_PATH) as conn:
            conn.execute("""
                INSERT INTO jobs (correlation_id, idempotency_key, priority, cost_center, payload, status)
                VALUES (?, ?, ?, ?, ?, 'PENDING')
            """, (c_id, idem_key, priority, "OPS", payload))
            
        return f"SUCCESS: OPS Cycle {c_id[:8]} queued."
    except Exception as e:
        return f"ERROR: {str(e)}"

if __name__ == "__main__":
    import sys
    msg = sys.argv[1] if len(sys.argv) > 1 else "Auto Sync"
    print(submit_ops_cycle(msg, 99))
