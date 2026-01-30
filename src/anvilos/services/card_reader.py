import time
import sqlite3
import json
import os
import sys
import subprocess
import uuid
from datetime import datetime
from pydantic import BaseModel, Field, ValidationError
from typing import Optional, Dict, Any, Union
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
DB_PATH = os.path.join(PROJECT_ROOT, "data", "cortex.db")
LOG_FILE = os.path.join(PROJECT_ROOT, "ext", "forge.log")
POLL_INTERVAL = 2.0
GIT_MAIN_BRANCH = "main"
class MicroLogger:
    def __init__(self, log_path):
        self.log_path = log_path
        os.makedirs(os.path.dirname(self.log_path), exist_ok=True)
    def log(self, event_id: int, data: Any):
        payload = {"@ID": event_id, "data": data, "ts": datetime.now().isoformat()}
        with open(self.log_path, "a") as f:
            f.write(json.dumps(payload) + "\n")
        print(f"[{event_id}] {data}")
logger = MicroLogger(LOG_FILE)
class JobCard(BaseModel):
    correlation_id: str
    idempotency_key: str
    priority: int = 50
    cost_center: str = "general"
    status: str = "PENDING"
    payload: Union[str, Dict[str, Any]]
    created_at: Optional[str] = None
class BigIronCore:
    def __init__(self, db_path):
        self.db_path = db_path
        if not os.path.exists(self.db_path):
            print(f"[CRITICAL] Database not found at {self.db_path}")
            sys.exit(1)
    def _get_conn(self):
        return sqlite3.connect(self.db_path)
    def fetch_next_job(self) -> Optional[JobCard]:
        """ Retrieves the highest priority PENDING job via Atomic Lock. """
        conn = self._get_conn()
        cursor = conn.cursor()
        try:
            conn.execute("BEGIN IMMEDIATE")
            cursor.execute("""
                SELECT correlation_id, payload, priority, cost_center, idempotency_key 
                FROM sys_jobs WHERE status = 'PENDING' 
                ORDER BY priority DESC, created_at ASC LIMIT 1
            """)
            row = cursor.fetchone()
            if row:
                c_id, payload_raw, prio, cost, idem_key = row
                cursor.execute("UPDATE sys_jobs SET status = 'PROCESSING', updated_at = ? WHERE correlation_id = ?", (datetime.now().isoformat(), c_id))
                conn.commit()
                try:
                    p_data = json.loads(payload_raw) if isinstance(payload_raw, str) else payload_raw
                except:
                    p_data = {"raw": payload_raw}
                return JobCard(correlation_id=c_id, idempotency_key=idem_key, priority=prio, cost_center=cost, status="PROCESSING", payload=p_data)
            else:
                conn.commit()
                return None
        except sqlite3.Error as e:
            print(f"[DB ERROR] {e}")
            conn.rollback()
            return None
        finally:
            conn.close()
    def update_job_status(self, correlation_id: str, status: str):
        conn = self._get_conn()
        try:
            conn.execute("UPDATE sys_jobs SET status = ?, updated_at = ? WHERE correlation_id = ?", (status, datetime.now().isoformat(), correlation_id))
            conn.commit()
        except Exception as e:
            print(f"[DB ERROR] Update failed: {e}")
        finally: conn.close()
    def run_cmd(self, args, description, check=True):
        """ Helper for cleaner subprocess calls """
        logger.log(5, f"OPS_STEP: {description}")
        res = subprocess.run(args, cwd=PROJECT_ROOT, capture_output=True, text=True)
        if check and res.returncode != 0:
            raise Exception(f"Command failed: {' '.join(args)}\nError: {res.stderr}")
        return res
    def execute_ops_lifecycle(self, message: str, correlation_id: str) -> bool:
        """
        OPS PROTOCOL: Branch -> Code (Already done) -> Commit -> Push -> PR -> Merge -> Clean
        """
        branch_name = f"ops/job-{correlation_id[:8]}"
        try:
            self.run_cmd(["git", "checkout", "-b", branch_name], "Creating Isolation Branch")
            self.run_cmd(["git", "add", "."], "Staging Artifacts")
            status = self.run_cmd(["git", "status", "--porcelain"], "Checking Status", check=False)
            if not status.stdout.strip():
                logger.log(4, "No changes to commit. Skipping lifecycle.")
                self.run_cmd(["git", "checkout", GIT_MAIN_BRANCH], "Returning to Main")
                self.run_cmd(["git", "branch", "-d", branch_name], "Deleting Empty Branch")
                return True
            self.run_cmd(["git", "commit", "-m", message], f"Committing: {message}")
            self.run_cmd(["git", "push", "-u", "origin", branch_name], "Pushing to Remote")
            pr_title = f"OPS: {message}"
            pr_body = f"Automated Action by Big Iron.\nJob ID: {correlation_id}"
            self.run_cmd([
                "gh", "pr", "create", 
                "--title", pr_title, 
                "--body", pr_body, 
                "--base", GIT_MAIN_BRANCH, 
                "--head", branch_name
            ], "Opening Pull Request")
            self.run_cmd([
                "gh", "pr", "merge", branch_name, 
                "--merge", 
                "--auto", 
                "--delete-branch"
            ], "Merging PR")
            self.run_cmd(["git", "checkout", GIT_MAIN_BRANCH], "Returning to Main")
            self.run_cmd(["git", "pull"], "Syncing Local Main")
            self.run_cmd(["git", "branch", "-D", branch_name], "Cleaning Local Branch")
            return True
        except Exception as e:
            logger.log(3, f"OPS FAILURE: {e}")
            try:
                subprocess.run(["git", "checkout", GIT_MAIN_BRANCH], cwd=PROJECT_ROOT, capture_output=True)
            except:
                pass
            return False
    def execute_logic(self, card: JobCard):
        logger.log(2, f"JOB_START: {card.correlation_id} ({card.priority})")
        data = card.payload if isinstance(card.payload, dict) else {}
        instruction = data.get("instruction", "UNKNOWN")
        if instruction == "GIT_COMMIT" or instruction == "OPS_CYCLE":
            msg = data.get("details", f"Ops Update {card.correlation_id[:8]}")
            return self.execute_ops_lifecycle(msg, card.correlation_id)
        elif instruction == "SYSTEM_OP":
            cmd = data.get("payload") or data.get("details")
            if not cmd:
                logger.log(4, "No command found in SYSTEM_OP payload.")
                return False
            logger.log(5, f"SYS_EXEC: {cmd}")
            res = subprocess.run(cmd, shell=True, cwd=PROJECT_ROOT, capture_output=True, text=True)
            if res.returncode == 0:
                logger.log(2, f"SYS_SUCCESS: {res.stdout.strip()}")
                return True
            else:
                logger.log(3, f"SYS_FAILURE: {res.stderr.strip()}")
                return False
        elif instruction == "SLEEP":
            time.sleep(2)
            return True
        elif instruction == "FAIL":
            return False
        else:
            logger.log(4, f"Unknown Instruction: {instruction}")
            return True
    def run(self):
        logger.log(1, f"BIG IRON v3.0 ONLINE. Watching {self.db_path}")
        while True:
            job = self.fetch_next_job()
            if job:
                try:
                    success = self.execute_logic(job)
                    status = "COMPLETE" if success else "FAILED"
                    self.update_job_status(job.correlation_id, status)
                    logger.log(2 if success else 3, f"JOB_{status}: {job.correlation_id}")
                except Exception as e:
                    logger.log(3, f"JOB_CRASHED: {job.correlation_id} - {e}")
                    self.update_job_status(job.correlation_id, "FAILED")
            else:
                time.sleep(POLL_INTERVAL)
if __name__ == "__main__":
    warden = BigIronCore(DB_PATH)
    if len(sys.argv) > 1:
        try:
            raw_input = sys.argv[1]
            data = json.loads(raw_input)
            instr = data.get("type", "UNKNOWN")
            if instr == "CODE_CHANGE": instr = "OPS_CYCLE"
            payload_data = {
                "instruction": instr,
                "details": data.get("payload"),
                "payload": data.get("payload"),
                "context": data.get("context")
            }
            card = JobCard(
                correlation_id=data.get("id", str(uuid.uuid4())),
                idempotency_key=str(uuid.uuid4()),
                status="PROCESSING",
                payload=payload_data
            )
            success = warden.execute_logic(card)
            sys.exit(0 if success else 1)
        except Exception as e:
            print(f"[CLI ERROR] {e}")
            sys.exit(1)
    else:
        warden.run()
