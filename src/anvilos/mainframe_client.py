#!/usr/bin/env python3

import sqlite3
import json
import time
import os

class MainframeClient:
    def __init__(self, db_path):
        self.db_path = db_path
        self._init_db()

    def _init_db(self):
        with sqlite3.connect(self.db_path) as conn:
            # 1. THE MAINFRAME STACK (Processor Daemon)
            conn.execute("""
                CREATE TABLE IF NOT EXISTS card_stack (
                    id TEXT PRIMARY KEY,
                    seq INTEGER,
                    op TEXT,
                    pld TEXT,
                    stat INTEGER DEFAULT 0,
                    ret TEXT,
                    timestamp REAL
                )
            """)
            # 2. THE CARD READER QUEUE (Big Iron / Card Reader Service)
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
            # 3. SYSTEM GOALS (Architect)
            conn.execute("""
                CREATE TABLE IF NOT EXISTS sys_goals (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    goal TEXT NOT NULL,
                    stat INTEGER DEFAULT 0,
                    timestamp REAL
                )
            """)
            # 4. LIVE TELEMETRY
            conn.execute("""
                CREATE TABLE IF NOT EXISTS live_stream (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    agent_id TEXT,
                    event_type TEXT,
                    details TEXT,
                    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
                )
            """)

    def get_stack_state(self):
        """Returns the current state of the Mainframe Card Stack with safety truncation."""
        try:
            with sqlite3.connect(self.db_path) as conn:
                conn.row_factory = sqlite3.Row
                
                # Optimized Counts
                pending_count = conn.execute("SELECT COUNT(*) FROM card_stack WHERE stat=0").fetchone()[0]
                processing_count = conn.execute("SELECT COUNT(*) FROM card_stack WHERE stat=1").fetchone()[0]
                
                # Get details safely
                def safe_dict(row):
                    if not row: return None
                    d = dict(row)
                    # Truncate large fields
                    if d.get('pld') and len(d['pld']) > 2000:
                        d['pld'] = d['pld'][:2000] + "... [TRUNCATED]"
                    if d.get('ret') and len(d['ret']) > 2000:
                        d['ret'] = d['ret'][:2000] + "... [TRUNCATED]"
                    return d

                # Get Active
                active_row = conn.execute("SELECT * FROM card_stack WHERE stat=1 LIMIT 1").fetchone()
                
                # Get Next
                next_row = conn.execute("SELECT * FROM card_stack WHERE stat=0 ORDER BY seq ASC LIMIT 1").fetchone()
                
                # Get Recent Failures
                failures_rows = conn.execute("SELECT * FROM card_stack WHERE stat=9 ORDER BY timestamp DESC LIMIT 5").fetchall()
                
                return {
                    "pending_count": pending_count,
                    "processing_count": processing_count,
                    "active_card": safe_dict(active_row),
                    "next_card": safe_dict(next_row),
                    "recent_failures": [safe_dict(r) for r in failures_rows]
                }
        except Exception as e:
            return {"error": str(e)}

    def inject_card(self, op, payload, seq=None, source="UNKNOWN"):
        """Injects a card directly into the Mainframe Stack."""
        import uuid
        card_id = str(uuid.uuid4())
        try:
            # SAFETY CHECK (RFC-0058): Prevent direct use of /usr/bin/gcc
            if op == "SYS_CMD" and "cmd" in payload:
                if "/usr/bin/gcc" in payload["cmd"] or "gcc " in payload["cmd"] and "musl-gcc" not in payload["cmd"]:
                    return {"status": "FAILURE", "error": "VIOLATION OF RFC-0058: Use of host GCC is forbidden. Use Anvil toolchain."}

            # Embed Source in Payload for Processor Daemon Validation
            if isinstance(payload, dict):
                payload["_source"] = source

            with sqlite3.connect(self.db_path) as conn:
                if seq is None:
                    # Append to end
                    last_seq = conn.execute("SELECT MAX(seq) FROM card_stack").fetchone()[0]
                    seq = (last_seq + 1) if last_seq is not None else 0
                
                conn.execute("""
                    INSERT INTO card_stack (id, seq, op, pld, stat, timestamp)
                    VALUES (?, ?, ?, ?, 0, ?)
                """, (card_id, seq, op, json.dumps(payload), time.time()))
            
            # STREAM TO CORTEX
            self.log_telemetry(source, "CARD_INJECTED", {"id": card_id, "op": op, "seq": seq})
            
            return {"status": "SUCCESS", "id": card_id, "seq": seq}
        except Exception as e:
            return {"status": "FAILURE", "error": str(e)}

    def inject_anvil(self, filename, source_code, target_binary, source="UNKNOWN"):
        """
        High-level helper to inject an Anvil build cycle:
        1. Write .anv file
        2. Transpile .anv to .c
        3. Compile .c to static binary
        """
        import os
        base_name = filename.rsplit('.', 1)[0]
        c_file = f"{base_name}.c"
        
        # 1. Write Source
        res1 = self.inject_card("FILE_WRITE", {"path": filename, "content": source_code}, source=source)
        if res1["status"] == "FAILURE": return res1
        
        # 2. Transpile
        transpile_cmd = f"python3 oss_sovereignty/sys_09_Anvil/source/anvil.py transpile {filename} {c_file}"
        res2 = self.inject_card("SYS_CMD", {"cmd": transpile_cmd, "timeout": 30}, source=source)
        if res2["status"] == "FAILURE": return res2
        
        # 3. Compile (Using Sovereign Toolchain)
        toolchain_gcc = "ext/toolchain/bin/x86_64-unknown-linux-musl-gcc"
        compile_cmd = f"{toolchain_gcc} -static -o {target_binary} {c_file}"
        res3 = self.inject_card("SYS_CMD", {"cmd": compile_cmd, "timeout": 60}, source=source)
        
        return {"status": "SUCCESS", "cycle_id": res1["id"]}

    def log_telemetry(self, agent_id, event_type, details):
        """Streams live telemetry to the Cortex."""
        try:
            with sqlite3.connect(self.db_path) as conn:
                conn.execute("""
                    INSERT INTO live_stream (agent_id, event_type, details, timestamp)
                    VALUES (?, ?, ?, CURRENT_TIMESTAMP)
                """, (agent_id, event_type, str(details)))
        except Exception as e:
            # Fallback to stderr if DB fails
            sys.stderr.write(f"[TELEMETRY FAIL] {e}\n")

    def submit_job(self, job_type, context, payload):
        """Submits a job to the Card Reader (Big Iron)."""
        import uuid
        c_id = str(uuid.uuid4())
        try:
            job_payload = {
                "instruction": "SYSTEM_OP" if job_type == "SYSTEM_OP" else "OPS_CYCLE",
                "type": job_type,
                "context": context,
                "details": payload,
                "payload": payload # Redundant but safe for reader compat
            }
            
            with sqlite3.connect(self.db_path) as conn:
                conn.execute("""
                    INSERT INTO sys_jobs (correlation_id, idempotency_key, payload, status)
                    VALUES (?, ?, ?, 'PENDING')
                """, (c_id, str(uuid.uuid4()), json.dumps(job_payload)))
            return {"status": "QUEUED", "job_id": c_id}
        except Exception as e:
            return {"status": "FAILURE", "error": str(e)}
