import sqlite3
import json
import time
import uuid
from typing import Dict, Any, Optional, List
class CortexDB:
    def __init__(self, db_path: str):
        self.db_path = db_path
        self._init_db()
    def _init_db(self):
        with sqlite3.connect(self.db_path) as conn:
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
            cursor = conn.execute("PRAGMA table_info(card_stack)")
            columns = [info[1] for info in cursor.fetchall()]
            if 'agent_id' not in columns:
                conn.execute("ALTER TABLE card_stack ADD COLUMN agent_id TEXT")
            if 'priority' not in columns:
                conn.execute("ALTER TABLE card_stack ADD COLUMN priority INTEGER DEFAULT 50")
            if 'parent_id' not in columns:
                conn.execute("ALTER TABLE card_stack ADD COLUMN parent_id TEXT")
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
                CREATE TABLE IF NOT EXISTS sys_goals (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    goal TEXT NOT NULL,
                    stat INTEGER DEFAULT 0,
                    timestamp REAL
                )
            """)
    def push_card(self, card_id: str, seq: int, op: str, pld: Dict[str, Any], agent_id: str = "system", priority: int = 50) -> None:
        """Pushes a new card onto the stack."""
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("""
                INSERT INTO card_stack (id, seq, op, pld, stat, timestamp, agent_id, priority)
                VALUES (?, ?, ?, ?, 0, ?, ?, ?)
            """, (card_id, seq, op, json.dumps(pld), time.time(), agent_id, priority))
    def pop_card(self) -> Optional[Dict[str, Any]]:
        """
        Retrieves the next pending card (stat=0), prioritizing higher priority,
        and atomically marks it as PROCESSING (stat=1).
        """
        with sqlite3.connect(self.db_path) as conn:
            conn.row_factory = sqlite3.Row
            query = """
                UPDATE card_stack 
                SET stat = 1 
                WHERE id = (
                    SELECT id FROM card_stack 
                    WHERE stat = 0 
                    ORDER BY priority DESC, timestamp ASC, seq ASC 
                    LIMIT 1
                )
                RETURNING *
            """
            try:
                row = conn.execute(query).fetchone()
                if row:
                    return dict(row)
            except sqlite3.OperationalError as e:
                if "RETURNING" in str(e):
                    conn.execute("BEGIN IMMEDIATE")
                    row = conn.execute("SELECT * FROM card_stack WHERE stat = 0 ORDER BY priority DESC, timestamp ASC, seq ASC LIMIT 1").fetchone()
                    if row:
                        conn.execute("UPDATE card_stack SET stat = 1 WHERE id = ?", (row['id'],))
                        conn.commit()
                        return dict(row)
                    conn.rollback()
                else:
                    raise e
        return None
    def peek_stack(self) -> Optional[Dict[str, Any]]:
        """Returns the next pending card without modifying it."""
        with sqlite3.connect(self.db_path) as conn:
            conn.row_factory = sqlite3.Row
            row = conn.execute("SELECT * FROM card_stack WHERE stat = 0 ORDER BY priority DESC, timestamp ASC, seq ASC LIMIT 1").fetchone()
            if row:
                return dict(row)
        return None
    def log_result(self, card_id: str, result: Dict[str, Any], status: int) -> None:
        """Updates the card status and return payload."""
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("UPDATE card_stack SET stat = ?, ret = ? WHERE id = ?", (status, json.dumps(result), card_id))
    def log_stream(self, agent_id: str, event_type: str, details: Dict[str, Any]) -> None:
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("""
                INSERT INTO live_stream (agent_id, event_type, details)
                VALUES (?, ?, ?)
            """, (agent_id, event_type, json.dumps(details)))
    def fetch_pending_goal(self) -> Optional[Dict[str, Any]]:
        with sqlite3.connect(self.db_path) as conn:
            conn.row_factory = sqlite3.Row
            row = conn.execute("SELECT * FROM sys_goals WHERE stat = 0 ORDER BY timestamp ASC LIMIT 1").fetchone()
            if row:
                return dict(row)
        return None
    def update_goal_status(self, goal_id: int, status: int) -> None:
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("UPDATE sys_goals SET stat = ? WHERE id = ?", (status, goal_id))
    def push_goal(self, goal: str) -> None:
        """Pushes a new high-level goal."""
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("""
                INSERT INTO sys_goals (id, goal, stat, timestamp)
                VALUES (?, ?, 0, ?)
            """, (str(uuid.uuid4()), goal, time.time()))

    def fetch_jammed_card(self) -> Optional[Dict[str, Any]]:
        """Fetches a single card with status 9 (Jam)."""
        with sqlite3.connect(self.db_path) as conn:
            conn.row_factory = sqlite3.Row
            row = conn.execute("SELECT * FROM card_stack WHERE stat = 9 ORDER BY timestamp ASC LIMIT 1").fetchone()
            if row:
                return dict(row)
        return None

    def update_card_status(self, card_id: str, status: int) -> None:
        """Updates just the status of a card."""
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("UPDATE card_stack SET stat = ? WHERE id = ?", (status, card_id))
