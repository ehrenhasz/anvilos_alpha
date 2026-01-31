#!/usr/bin/env python3
import sqlite3
import os
import json
import sys

# Determine paths
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DB_PATH = os.path.join(BASE_DIR, "data", "cortex.db")

def get_status():
    if not os.path.exists(DB_PATH):
        return {"error": "Database not found", "path": DB_PATH}

    try:
        conn = sqlite3.connect(DB_PATH)
        cursor = conn.cursor()
        
        # Check if table exists
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='card_stack'")
        if not cursor.fetchone():
             return {"error": "Table 'card_stack' not found in DB."}

        cursor.execute("SELECT stat, COUNT(*) FROM card_stack GROUP BY stat")
        rows = cursor.fetchall()
        
        stats = {
            "0": 0, # PENDING
            "2": 0, # PUNCHED
            "9": 0  # JAMMED
        }
        
        total = 0
        for stat, count in rows:
            stats[str(stat)] = count
            total += count
            
        # Determine overall state
        jammed = stats.get("9", 0)
        pending = stats.get("0", 0)
        
        if jammed > 0:
            state = "JAMMED"
        elif pending > 0:
            state = "FORGING"
        else:
            state = "IDLE"

        return {
            "system": "ANVILOS",
            "state": state,
            "total_cards": total,
            "card_stack": {
                "PENDING": stats.get("0", 0),
                "PUNCHED": stats.get("2", 0),
                "JAMMED": stats.get("9", 0)
            }
        }
    except Exception as e:
        return {"error": str(e)}
    finally:
        if 'conn' in locals():
            conn.close()

if __name__ == "__main__":
    # Always print status for now, ignoring specific args unless we expand functionality
    print(json.dumps(get_status(), indent=2))
