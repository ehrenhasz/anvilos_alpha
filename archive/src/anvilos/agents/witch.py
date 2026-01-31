import sqlite3
import hashlib
import json
import time
from typing import Dict, Any, Optional
import os
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
CORTEX_DB_PATH = os.path.join(PROJECT_ROOT, "data", "cortex.db")
class Witch:
    def __init__(self, db_path: str = None):
        self.db_path = db_path or CORTEX_DB_PATH
        self._init_db()
    def _init_db(self):
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS artifact_ledger (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    artifact_hash TEXT NOT NULL,
                    path TEXT NOT NULL,
                    status TEXT DEFAULT 'PENDING',
                    signature TEXT,
                    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
                )
            """)
    def fetch_pending_artifacts(self):
        with sqlite3.connect(self.db_path) as conn:
            conn.row_factory = sqlite3.Row
            rows = conn.execute("SELECT * FROM artifact_ledger WHERE status = 'PENDING'").fetchall()
            return [dict(row) for row in rows]
    def sign_artifact(self, artifact_id: int, secret_key: str) -> bool:
        """
        Signs an artifact hash with a secret key (mocked for now).
        """
        try:
            with sqlite3.connect(self.db_path) as conn:
                row = conn.execute("SELECT artifact_hash FROM artifact_ledger WHERE id = ?", (artifact_id,)).fetchone()
                if not row:
                    return False
                artifact_hash = row[0]
                signature = hashlib.sha256((artifact_hash + secret_key).encode()).hexdigest()
                conn.execute("UPDATE artifact_ledger SET status = 'SIGNED', signature = ? WHERE id = ?", (signature, artifact_id))
                return True
        except Exception as e:
            print(f"[Witch Error] {e}")
            return False
    def reject_artifact(self, artifact_id: int, reason: str) -> bool:
        try:
            with sqlite3.connect(self.db_path) as conn:
                conn.execute("UPDATE artifact_ledger SET status = 'REJECTED', signature = ? WHERE id = ?", (reason, artifact_id))
                return True
        except Exception as e:
            print(f"[Witch Error] {e}")
            return False
    def register_artifact(self, path: str) -> Optional[int]:
        """Calculates hash and registers an artifact for ratification."""
        try:
            with open(path, "rb") as f:
                artifact_hash = hashlib.sha256(f.read()).hexdigest()
            with sqlite3.connect(self.db_path) as conn:
                cursor = conn.execute("INSERT INTO artifact_ledger (artifact_hash, path) VALUES (?, ?)", (artifact_hash, path))
                return cursor.lastrowid
        except Exception as e:
            print(f"[Witch Error] {e}")
            return None
if __name__ == "__main__":
    witch = Witch()
    print("The Witch is watching...")
    while True:
        pending = witch.fetch_pending_artifacts()
        if pending:
            print(f"Pending Artifacts: {len(pending)}")
            for artifact in pending:
                print(f"Rejecting {artifact['path']}")
                witch.reject_artifact(artifact['id'], "Auto-Reject: No manual override")
        time.sleep(5)
