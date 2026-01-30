import sqlite3
import os
import sys
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
SYSTEM_DB = os.path.join(PROJECT_ROOT, "data", "cortex.db")
def init_mainframe(db_path):
    os.makedirs(os.path.dirname(db_path), exist_ok=True)
    print(f"[INIT] Initializing Mainframe on {db_path}...")
    try:
        with sqlite3.connect(db_path) as conn:
            conn.execute("DROP TABLE IF EXISTS card_stack")
            conn.execute("""
                CREATE TABLE card_stack (
                    id TEXT PRIMARY KEY,
                    seq INTEGER,
                    op TEXT,
                    pld TEXT,
                    ret TEXT,
                    stat INTEGER,
                    timestamp REAL
                )
            """)
            conn.execute("DROP TABLE IF EXISTS sys_goals")
            conn.execute("""
                CREATE TABLE sys_goals (
                    id TEXT PRIMARY KEY,
                    goal TEXT,
                    stat INTEGER DEFAULT 0,
                    timestamp REAL
                )
            """)
            print(f"[OK] card_stack and sys_goals tables created.")
    except Exception as e:
        print(f"[ERROR] Failed to init {db_path}: {e}")
if __name__ == "__main__":
    init_mainframe(SYSTEM_DB)
