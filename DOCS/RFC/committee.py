import os
import sys
import sqlite3
import json
import datetime
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(CURRENT_DIR, '..', '..'))
RUNTIME_DIR = os.path.join(PROJECT_ROOT, "runtime")
DEFAULT_DB_PATH = "/var/lib/anvilos/db/cortex.db"
LOCAL_DB_PATH = os.path.join(PROJECT_ROOT, "data", "cortex.db")
if os.path.exists(DEFAULT_DB_PATH):
    DB_PATH = DEFAULT_DB_PATH
elif os.path.exists(LOCAL_DB_PATH):
    DB_PATH = LOCAL_DB_PATH
else:
    DB_PATH = DEFAULT_DB_PATH # Fallback to default for error reporting
sys.path.append(RUNTIME_DIR)
class Committee:
    def __init__(self):
        self.db_path = DB_PATH
        self.chairman = "bigiron" # $meat
        self.vice_chair = "aimeat" # $aimeat
        self.observer = "thespy" # $thespy
    def _get_connection(self):
        """Establishes connection to the central Cortex DB."""
        if not os.path.exists(self.db_path):
            print(f"[!] Critical: Cortex DB not found at {self.db_path}")
            return None
        return sqlite3.connect(self.db_path)
    def log_quorum(self, status="OPENED", notes=""):
        """Logs the Committee Session to the DB."""
        conn = self._get_connection()
        if not conn: return
        try:
            cursor = conn.cursor()
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS committee_log (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                    action TEXT,
                    participants TEXT,
                    notes TEXT
                )
            """)
            participants = json.dumps([self.chairman, self.vice_chair, self.observer])
            cursor.execute("INSERT INTO committee_log (action, participants, notes) VALUES (?, ?, ?)",
                           (f"QUORUM_{status}", participants, notes))
            try:
                cursor.execute("""
                    CREATE TABLE IF NOT EXISTS live_stream (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        agent_id TEXT,
                        event_type TEXT,
                        details TEXT,
                        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
                    )
                """)
                cursor.execute("INSERT INTO live_stream (agent_id, event_type, details) VALUES (?, ?, ?)",
                               ("COMMITTEE", f"QUORUM_{status}", notes))
            except Exception: pass
            conn.commit()
            print(f"[*] Quorum {status} logged to Cortex.")
        except Exception as e:
            print(f"[!] DB Error: {e}")
        finally:
            conn.close()
    def check_aimeat_status(self):
        """Checks the status of the Vice-Chair (AImeat) in the DB."""
        conn = self._get_connection()
        if not conn: return "UNKNOWN"
        try:
            cursor = conn.cursor()
            cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='agents'")
            if not cursor.fetchone():
                return "OFFLINE (No Table)"
            cursor.execute("SELECT status, updated_at FROM agents WHERE agent_id=?", (self.vice_chair,))
            row = cursor.fetchone()
            if row:
                return f"{row[0]} (Last Seen: {row[1]})"
            else:
                return "OFFLINE (Not Registered)"
        except Exception as e:
            return f"ERROR ({e})"
        finally:
            conn.close()
    def list_awaiting_rfcs(self):
        """Scans the directory for unratified RFCs."""
        print("\n--- DOCKET (Draft Protocol Scan) ---")
        found = False
        for f in os.listdir(CURRENT_DIR):
            if f.endswith(".txt"):
                path = os.path.join(CURRENT_DIR, f)
                try:
                    with open(path, 'r', errors='ignore') as rfc_file:
                        head = [next(rfc_file) for _ in range(20)]
                        is_draft = False
                        for line in head:
                            if "STATUS:" in line and ("DRAFT" in line or "PENDING" in line):
                                is_draft = True
                                break
                        if is_draft:
                            print(f"[ ] {f} (STATUS: DRAFT)")
                            found = True
                except Exception as e:
                    pass
        awaiting_dir = os.path.join(CURRENT_DIR, "Awaiting")
        if os.path.exists(awaiting_dir) and os.path.isdir(awaiting_dir):
            for f in os.listdir(awaiting_dir):
                print(f"[ ] Awaiting/{f}")
                found = True
        if not found:
            print("(No new items on the docket)")
def call_quorum():
    """Main routine to convene the Committee."""
    os.system('cls' if os.name == 'nt' else 'clear')
    print("###################################################")
    print("#           THE BICAMERAL COMMITTEE               #")
    print("#          Quorum Call Initiated...               #")
    print("###################################################")
    comm = Committee()
    print(f"\n[*] Chairman ($meat):   PRESENT ({comm.chairman})")
    aimeat_status = comm.check_aimeat_status()
    print(f"[*] Vice-Chair ($aimeat): {aimeat_status}")
    print(f"[*] Observer ($thespy):   ACTIVE (Passive Monitoring)")
    comm.log_quorum(status="CALLED", notes="Session initiated via CLI")
    comm.list_awaiting_rfcs()
    print("\n[!] The Committee is in session. Commands available via 'submit_ops_cycle'.")
if __name__ == "__main__":
    call_quorum()
