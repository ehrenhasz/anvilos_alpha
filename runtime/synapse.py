import sqlite3
import os
import json
import time
from datetime import datetime

# --- PATH RESOLUTION ---
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SYSTEM_DB = "/var/lib/anvilos/db/cortex.db"
APP_DB = "/var/lib/anvilos/db/dnd_dm.db"

class Synapse:
    def __init__(self, agent_id):
        self.agent_id = agent_id
        # Local DB remains for agent-specific temporary state, but we could move this too eventually.
        self.local_db = os.path.join(PROJECT_ROOT, "runtime", f"agent_{agent_id}.db")
        self._init_local_db()

    def _get_local_conn(self):
        return sqlite3.connect(self.local_db)

    def _get_cortex_conn(self):
        """
        Connects to the System Brain and attaches the World (App DB).
        """
        try:
            conn = sqlite3.connect(SYSTEM_DB)
            if os.path.exists(APP_DB):
                conn.execute(f"ATTACH DATABASE '{APP_DB}' AS dnd")
            return conn
        except sqlite3.Error as e:
            print(f"[SYNAPSE] Connection Error: {e}")
            return sqlite3.connect(SYSTEM_DB)

    def _init_local_db(self):
        with self._get_local_conn() as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS local_jobs (
                    id TEXT PRIMARY KEY,
                    status TEXT,
                    payload TEXT,
                    result TEXT
                )
            """)
            conn.execute("""
                CREATE TABLE IF NOT EXISTS event_log (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    event_type TEXT,
                    context TEXT,
                    success BOOLEAN,
                    details TEXT,
                    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                    synced BOOLEAN DEFAULT 0
                )
            """)

    def log_experience(self, task_type, context, success, details):
        try:
            with self._get_local_conn() as conn:
                conn.execute("""
                    INSERT INTO event_log (event_type, context, success, details)
                    VALUES (?, ?, ?, ?)
                """, (task_type, context, 1 if success else 0, json.dumps(details)))
            self.sync_to_cortex()
        except Exception as e:
            print(f"[SYNAPSE] Log Error: {e}")

    def sync_to_cortex(self):
        try:
            local_conn = self._get_local_conn()
            local_cursor = local_conn.cursor()
            local_cursor.execute("SELECT id, event_type, context, success, details, timestamp FROM event_log WHERE synced=0")
            logs = local_cursor.fetchall()
            
            if not logs: return

            cortex_conn = self._get_cortex_conn()
            cortex_conn.execute("""
                CREATE TABLE IF NOT EXISTS experience_log (
                    id TEXT PRIMARY KEY,
                    agent_id TEXT,
                    event_type TEXT,
                    context TEXT,
                    success BOOLEAN,
                    details TEXT,
                    timestamp DATETIME
                )
            """)

            # Ensure sys_agents exists (it should from migration)
            cortex_conn.execute("""
                CREATE TABLE IF NOT EXISTS sys_agents (
                    agent_id TEXT PRIMARY KEY,
                    status TEXT,
                    updated_at DATETIME,
                    coding_id TEXT
                )
            """)
            
            for row in logs:
                log_id, et, ctx, suc, det, ts = row
                global_id = f"{self.agent_id}-{log_id}-{ts}"
                cortex_conn.execute("""
                    INSERT OR IGNORE INTO experience_log (id, agent_id, event_type, context, success, details, timestamp)
                    VALUES (?, ?, ?, ?, ?, ?, ?)
                """, (global_id, self.agent_id, et, ctx, suc, det, ts))
                
                # Update Status in sys_agents table
                cortex_conn.execute("INSERT OR REPLACE INTO sys_agents (agent_id, status, updated_at) VALUES (?, 'ONLINE', CURRENT_TIMESTAMP)", (self.agent_id,))
                
                local_conn.execute("UPDATE event_log SET synced=1 WHERE id=?", (log_id,))
            
            cortex_conn.commit()
            local_conn.commit()
        except Exception as e:
            print(f"[SYNAPSE] Sync Failed: {e}")
        finally:
            if 'local_conn' in locals(): local_conn.close()
            if 'cortex_conn' in locals(): cortex_conn.close()

    def get_job(self):
        try:
            cortex = self._get_cortex_conn()
            cortex.row_factory = sqlite3.Row
            # Debug
            # print(f"[SYNAPSE] Checking for jobs for {self.agent_id}...")
            # Using sys_jobs as per new schema
            cursor = cortex.execute("SELECT * FROM sys_jobs WHERE status='PENDING' ORDER BY id ASC LIMIT 1")
            job = cursor.fetchone()
            if job:
                # correlation_id might not be in sys_jobs if not added, but the migration added it.
                # However, previous sys_jobs didn't have 'worker'. It was a simple queue.
                # Let's adapt to the simple sys_jobs schema from migrate_cortex.py:
                # id, command, payload, status, created_at, processed_at, correlation_id
                
                job_id = job['id']
                corr_id = job['correlation_id'] if job['correlation_id'] else str(job_id)
                
                print(f"[SYNAPSE] Claiming Job: {corr_id}")
                
                cortex.execute("UPDATE sys_jobs SET status='PROCESSING' WHERE id=?", (job_id,))
                cortex.commit()
                
                with self._get_local_conn() as local:
                    local.execute("INSERT OR REPLACE INTO local_jobs (id, status, payload) VALUES (?, 'PROCESSING', ?)", (corr_id, job['payload']))
                
                # Return dict but map id to correlation_id for compatibility if needed
                return {
                    "correlation_id": corr_id,
                    "payload": job['payload'],
                    "command": job['command']
                }
            else:
                # print("[SYNAPSE] No matching jobs found.")
                pass
            return None
        except Exception as e:
            print(f"[SYNAPSE] Job Fetch Error: {e}")
            return None

    def update_job(self, job_id, status, result):
        with self._get_local_conn() as local:
            local.execute("UPDATE local_jobs SET status=?, result=? WHERE id=?", (status, str(result), job_id))
        try:
            with self._get_cortex_conn() as cortex:
                # We need to find the job by correlation_id or id. 
                # If job_id passed here is correlation_id, we need to query by that.
                cortex.execute("UPDATE sys_jobs SET status=?, processed_at=CURRENT_TIMESTAMP WHERE correlation_id=?", (status, job_id))
                cortex.commit()
        except Exception as e:
            print(f"[SYNAPSE] Cortex Update Error: {e}")
