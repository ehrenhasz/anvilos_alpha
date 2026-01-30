import sys
import os
import sqlite3
import json
import uuid

# Add root to path
sys.path.append(os.getcwd())
try:
    import forge_directives
except ImportError:
    print("Error: forge_directives.py not found.")
    sys.exit(1)

DB_PATH = "data/cortex.db"

def inject():
    if not hasattr(forge_directives, "FULL_STACK"):
        print("Error: forge_directives.FULL_STACK not found.")
        sys.exit(1)
        
    cards = forge_directives.FULL_STACK
    print(f"Injecting {len(cards)} cards from forge_directives.FULL_STACK...")
    
    try:
        with sqlite3.connect(DB_PATH) as conn:
            cursor = conn.cursor()
            for card in cards:
                card_id = str(uuid.uuid4())
                seq = card.get("seq", 999)
                op = card.get("op", "unknown_op")
                pld_dict = card.get("pld", {})
                
                # Sign the card
                pld_dict["_source"] = "COMMANDER"
                pld = json.dumps(pld_dict)
                
                cursor.execute("""
                    INSERT INTO card_stack (id, seq, op, pld, stat, agent_id, timestamp)
                    VALUES (?, ?, ?, ?, 0, 'AIMEAT', CURRENT_TIMESTAMP)
                """, (card_id, seq, op, pld))
            conn.commit()
        print(f"SUCCESS: Injected {len(cards)} cards.")
    except Exception as e:
        print(f"FAILURE: {e}")

if __name__ == "__main__":
    inject()
