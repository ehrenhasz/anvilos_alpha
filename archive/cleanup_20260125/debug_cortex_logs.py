import sqlite3
import json
import os
import sys

db_path = "runtime/cortex.db"

if not os.path.exists(db_path):
    print(f"Error: {db_path} not found.")
    sys.exit(1)

conn = sqlite3.connect(db_path)
conn.row_factory = sqlite3.Row

print("--- RECENT CARD STACK OPERATIONS ---")
try:
    cursor = conn.execute("SELECT id, op, pld, ret, stat, timestamp FROM card_stack WHERE stat != 0 ORDER BY timestamp DESC LIMIT 20")
    rows = cursor.fetchall()
    for row in rows:
        print(f"ID: {row['id']}")
        print(f"OP: {row['op']}")
        print(f"Status: {row['stat']}")
        try:
            pld = json.loads(row['pld'])
            # Truncate large payloads
            pld_str = json.dumps(pld, indent=2)
            if len(pld_str) > 500:
                pld_str = pld_str[:500] + "... [TRUNCATED]"
            print(f"Payload: {pld_str}")
        except:
            print(f"Payload: {row['pld']}")
        
        if row['ret']:
            try:
                ret = json.loads(row['ret'])
                ret_str = json.dumps(ret, indent=2)
                if len(ret_str) > 500:
                    ret_str = ret_str[:500] + "... [TRUNCATED]"
                print(f"Return: {ret_str}")
            except:
                print(f"Return: {row['ret']}")
        print("-" * 40)
except Exception as e:
    print(f"Error reading card_stack: {e}")

print("\n--- RECENT LIVE STREAM EVENTS ---")
try:
    cursor = conn.execute("SELECT agent_id, event_type, details, timestamp FROM live_stream ORDER BY timestamp DESC LIMIT 20")
    rows = cursor.fetchall()
    for row in rows:
        print(f"[{row['timestamp']}] {row['agent_id']} - {row['event_type']}")
        print(f"Details: {row['details']}")
        print("-" * 20)
except Exception as e:
    print(f"Error reading live_stream: {e}")

conn.close()
