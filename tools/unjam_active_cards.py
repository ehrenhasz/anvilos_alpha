import sqlite3
import json
import os
DB_PATH = "data/cortex.db"
def fix_db_jams():
    print(f"Connecting to {DB_PATH}...")
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cursor = conn.cursor()
    cursor.execute("SELECT id, pld, ret FROM card_stack WHERE stat = 9")
    rows = cursor.fetchall()
    print(f"Found {len(rows)} jammed cards.")
    fixed_count = 0
    for row in rows:
        card_id = row['id']
        pld_str = row['pld']
        ret_str = row['ret']
        try:
            pld = json.loads(pld_str)
        except Exception as e:
            print(f"Skipping malformed card {card_id}: {e}")
            continue
        pld['_source'] = "COMMANDER"
        new_pld_str = json.dumps(pld)
        cursor.execute("UPDATE card_stack SET stat = 0, pld = ?, ret = NULL WHERE id = ?", (new_pld_str, card_id))
        fixed_count += 1
        print(f"Fixed card {card_id} (stat 9 -> 0, added _source)")
    conn.commit()
    conn.close()
    print(f"Total fixed cards: {fixed_count}")
if __name__ == "__main__":
    fix_db_jams()
