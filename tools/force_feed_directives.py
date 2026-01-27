import sqlite3
import uuid
import re
import time

DB_PATH = "data/cortex.db"
FILE_PATH = "PHASE2_DIRECTIVES.md"

def parse_and_feed():
    with open(FILE_PATH, 'r') as f:
        content = f.read()

    # Regex to find cards: "- [ ] **Card 100:** `fetch_zfs_source` (Download...)"
    # We want to capture the whole line as the payload.
    # We only want unchecked ones? The prompt says "for each directive". 
    # Card 100 is checked "- [x]". User said "for each directive". 
    # I will include all of them, but maybe mark checked ones as DONE (stat=2)?
    # User said "Set stat=0 (PENDING)". I will follow that strictly.
    
    # Matches: - [x] **Card ...
    # or       - [ ] **Card ...
    pattern = re.compile(r'^- \[[ x]\] \*\*Card (\d+):\*\* (.*)$', re.MULTILINE)
    
    matches = pattern.findall(content)
    
    print(f"Found {len(matches)} directives.")
    
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    
    # Get current max sequence
    cursor.execute("SELECT MAX(seq) FROM card_stack")
    res = cursor.fetchone()
    current_seq = res[0] if res[0] is not None else 0
    
    print(f"Current max sequence: {current_seq}")
    
    for i, (card_num, text) in enumerate(matches):
        card_id = str(uuid.uuid4())
        seq = current_seq + 1 + i
        # Reconstruct the full text for the payload
        full_text = f"Card {card_num}: {text}"
        
        # We use 'DIRECTIVE' as the operation
        op = "DIRECTIVE"
        
        print(f"Inserting Card {card_num} as seq {seq}...")
        cursor.execute(
            "INSERT INTO card_stack (id, seq, op, pld, stat, timestamp) VALUES (?, ?, ?, ?, ?, ?)",
            (card_id, seq, op, full_text, 0, time.time())
        )
        
    conn.commit()
    conn.close()
    print("Force feed complete.")

if __name__ == "__main__":
    parse_and_feed()
