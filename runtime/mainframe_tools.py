import json
import uuid
import time
from .cortex.db_interface import CortexDB

# Initialize Shared DB Connection
CORTEX_DB_PATH = "/var/lib/anvilos/db/cortex.db"
MAINFRAME = CortexDB(CORTEX_DB_PATH)

def get_stack_state():
    """Returns the current state of the Mainframe Card Stack."""
    try:
        # We need a direct SQL call for aggregate stats
        import sqlite3
        with sqlite3.connect(CORTEX_DB_PATH) as conn:
            conn.row_factory = sqlite3.Row
            pending_count = conn.execute("SELECT COUNT(*) FROM card_stack WHERE stat=0").fetchone()[0]
            processing_count = conn.execute("SELECT COUNT(*) FROM card_stack WHERE stat=1").fetchone()[0]
            
            # Helper for safe dict conversion
            def safe_dict(row):
                if not row: return None
                d = dict(row)
                if d.get('pld') and len(d['pld']) > 2000:
                    d['pld'] = d['pld'][:2000] + "... [TRUNCATED]"
                if d.get('ret') and len(d['ret']) > 2000:
                    d['ret'] = d['ret'][:2000] + "... [TRUNCATED]"
                return d

            active_row = conn.execute("SELECT * FROM card_stack WHERE stat=1 LIMIT 1").fetchone()
            next_row = conn.execute("SELECT * FROM card_stack WHERE stat=0 ORDER BY priority DESC, seq ASC LIMIT 1").fetchone()
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

def inject_card(op: str, payload: dict, agent_id: str = "system", priority: int = 50):
    """
    Injects a card into the cortex.
    """
    card_id = str(uuid.uuid4())
    try:
        # Get last seq
        import sqlite3
        with sqlite3.connect(CORTEX_DB_PATH) as conn:
            last_seq = conn.execute("SELECT MAX(seq) FROM card_stack").fetchone()[0]
            seq = (last_seq + 1) if last_seq is not None else 0
            
        MAINFRAME.push_card(card_id, seq, op, payload, agent_id, priority)
        return {"status": "SUCCESS", "id": card_id, "seq": seq}
    except Exception as e:
        return {"status": "FAILURE", "error": str(e)}

def inject_anvil(filename: str, source_code: str, target_binary: str, agent_id: str = "system"):
    """
    Helper for the Anvil Build Cycle.
    """
    base_name = filename.rsplit('.', 1)[0]
    c_file = f"{base_name}.c"
    
    # 1. Write Source
    res1 = inject_card("FILE_WRITE", {"path": filename, "content": source_code}, agent_id)
    if res1["status"] == "FAILURE": return res1
    
    # 2. Transpile
    transpile_cmd = f"python3 oss_sovereignty/sys_09_Anvil/source/anvil.py transpile {filename} {c_file}"
    res2 = inject_card("SYS_CMD", {"cmd": transpile_cmd, "timeout": 30}, agent_id)
    if res2["status"] == "FAILURE": return res2
    
    # 3. Compile
    toolchain_gcc = "ext/toolchain/bin/x86_64-unknown-linux-musl-gcc"
    compile_cmd = f"{toolchain_gcc} -static -o {target_binary} {c_file}"
    res3 = inject_card("SYS_CMD", {"cmd": compile_cmd, "timeout": 60}, agent_id)
    
    return {"status": "SUCCESS", "cycle_id": res1["id"]}
