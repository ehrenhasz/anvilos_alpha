import sqlite3
import os
DB_PATH = "data/cortex.db"
def repair_db():
    print(f"Repairing {DB_PATH}...")
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute("""
            CREATE TABLE IF NOT EXISTS chat_inbox (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                agent_id TEXT,
                message TEXT,
                status TEXT DEFAULT 'PENDING',
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        """)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS chat_outbox (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                message TEXT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        """)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS sys_jobs (
                correlation_id TEXT PRIMARY KEY,
                idempotency_key TEXT,
                priority INTEGER DEFAULT 50,
                cost_center TEXT,
                status TEXT DEFAULT 'PENDING',
                payload TEXT,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                updated_at DATETIME
            )
        """)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS live_stream (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                agent_id TEXT,
                event_type TEXT,
                details TEXT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        """)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS card_stack (
                id TEXT PRIMARY KEY,
                seq INTEGER,
                op TEXT,
                pld TEXT,
                stat INTEGER DEFAULT 0,
                ret TEXT,
                timestamp REAL,
                agent_id TEXT,
                priority INTEGER DEFAULT 50,
                parent_id TEXT
            )
        """)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS sys_goals (
                id TEXT PRIMARY KEY,
                goal TEXT,
                stat INTEGER DEFAULT 0,
                timestamp REAL
            )
        """)
        print("Schema repair complete.")
if __name__ == "__main__":
    repair_db()
