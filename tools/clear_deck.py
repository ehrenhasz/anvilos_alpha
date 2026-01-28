import sys
import sqlite3
import os
DB_PATH = "data/cortex.db"
def clear_deck():
    if not os.path.exists(DB_PATH):
        print(f"Database not found at {DB_PATH}")
        return
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='card_stack'")
    if not cursor.fetchone():
        print("Table 'card_stack' does not exist.")
        conn.close()
        return
    cursor.execute("SELECT COUNT(*) FROM card_stack")
    count = cursor.fetchone()[0]
    print(f"Cards before clear: {count}")
    cursor.execute("DELETE FROM card_stack")
    conn.commit()
    cursor.execute("SELECT COUNT(*) FROM card_stack")
    count_after = cursor.fetchone()[0]
    print(f"Cards after clear: {count_after}")
    conn.close()
if __name__ == "__main__":
    clear_deck()
