import sqlite3
import uuid
import re
import time
DB_PATH = "data/cortex.db"
FILE_PATH = "PHASE2_DIRECTIVES.md"
def parse_and_feed():
    with open(FILE_PATH, 'r') as f:
        content = f.read()
    pattern = re.compile(r'^- \[[ x]\] \*\*Card (\d+):\*\* (.*)$', re.MULTILINE)
    matches = pattern.findall(content)
    print(f"Found {len(matches)} directives.")
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("SELECT MAX(seq) FROM card_stack")
    res = cursor.fetchone()
    current_seq = res[0] if res[0] is not None else 0
    print(f"Current max sequence: {current_seq}")
    for i, (card_num, text) in enumerate(matches):
        card_id = str(uuid.uuid4())
        seq = current_seq + 1 + i
        full_text = f"Card {card_num}: {text}"
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
