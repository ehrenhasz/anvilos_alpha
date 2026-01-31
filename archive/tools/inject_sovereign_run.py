import sqlite3
import json
import uuid
import forge_directives

DB_PATH = "data/cortex.db"

def inject():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    
    print("Clearing debris...")
    cursor.execute("DELETE FROM card_stack")
    conn.commit()
    
    print("Injecting Sovereign Population...")
    directives = forge_directives.PHASE2_DIRECTIVES
    
    batch = []
    for card in directives:
        card_id = str(uuid.uuid4())
        seq = card.get("seq", 999)
        op = card.get("op", "unknown")
        pld = card.get("pld", {})
        pld["_source"] = "COMMANDER"
        
        batch.append((card_id, seq, op, json.dumps(pld), "AIMEAT"))
        
    cursor.executemany("""
        INSERT INTO card_stack (id, seq, op, pld, stat, ret, timestamp, agent_id)
        VALUES (?, ?, ?, ?, 0, NULL, CURRENT_TIMESTAMP, ?)
    """, batch)
    
    conn.commit()
    print(f"Injected {len(batch)} cards.")
    conn.close()

if __name__ == "__main__":
    inject()
