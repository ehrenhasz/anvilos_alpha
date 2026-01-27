#!/usr/bin/env python3
import os
import sys
import sqlite3
import json
import subprocess
import time
import logging

# --- PATH RESOLUTION ---
PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
SYSTEM_DB = os.path.join(PROJECT_ROOT, "data", "cortex.db")
sys.path.append(os.path.join(PROJECT_ROOT, "src"))
sys.path.append(os.path.join(PROJECT_ROOT, "vendor"))

from anvilos.cortex.db_interface import CortexDB

# --- LOGGING ---
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - [PROCESSOR] - %(levelname)s - %(message)s'
)
logger = logging.getLogger()

DB = CortexDB(SYSTEM_DB)

# Ensure consistent execution context
os.chdir(PROJECT_ROOT)

def execute_sys_cmd(pld):
    cmd = pld.get("cmd")
    timeout = pld.get("timeout", 3600)
    
    # Pre-processing: if command uses /dev/loop0, try to find a free one
    if "/dev/loop0" in cmd:
        try:
            free_loop = subprocess.check_output("sudo losetup -f", shell=True, text=True).strip()
            cmd = cmd.replace("/dev/loop0", free_loop)
            logger.info(f"Remapped /dev/loop0 to {free_loop}")
        except Exception as e:
            logger.warn(f"Failed to find free loop device: {e}")

    logger.info(f"Executing System Command: {cmd} (timeout={timeout}s)")
    try:
        res = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)
        return {
            "stdout": res.stdout[-10000:], # Keep only last 10k chars to avoid DB bloat
            "stderr": res.stderr[-10000:],
            "exit_code": res.returncode
        }
    except subprocess.TimeoutExpired as e:
        logger.error(f"Command Timed Out: {e}")
        return {
            "error": "Timeout",
            "stdout": (e.stdout.decode() if e.stdout else "")[-10000:],
            "stderr": (e.stderr.decode() if e.stderr else "")[-10000:]
        }
    except Exception as e:
        return {"error": str(e)}

def execute_file_write(pld):
    path = pld.get("path")
    content = pld.get("content")
    # Resolve relative paths relative to Project Root for safety
    if not os.path.isabs(path):
        path = os.path.join(PROJECT_ROOT, path)
    
    logger.info(f"Writing to File: {path}")
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            f.write(content)
        return {"status": "SUCCESS", "path": path}
    except Exception as e:
        return {"error": str(e)}


# --- WARDEN'S LOGIC (RFC-0058 ENFORCEMENT) ---
def _validate_card(op, pld):
    """
    The Warden: scrutinizes the card for violations of The Law of Anvil.
    Returns: (bool, str) -> (Allowed?, Reason)
    """
    # 0. Source Verification (The Collar)
    # Cards must be signed by the Forge or the Commander.
    source = pld.get("_source", "UNKNOWN")
    if source not in ["FORGE", "COMMANDER"]:
        return False, f"VIOLATION: Untrusted Source '{source}'. Only FORGE or COMMANDER may inject cards."

    if op == "SYS_CMD":
        cmd = pld.get("cmd", "")
        # 1. Dirty Tools Prohibition
        forbidden = ["/usr/bin/gcc", "gcc ", "/usr/bin/clang", "clang ", "/usr/bin/rustc", "rustc ", "cargo build"]
        # Allow sovereign toolchain
        allowed_prefix = "ext/toolchain"
        
        for tool in forbidden:
            if tool in cmd and allowed_prefix not in cmd:
                return False, f"VIOLATION: Forbidden tool '{tool}' detected. Use Sovereign Toolchain."
        
        # 2. Safety Barriers
        # More precise check: reject "rm -rf /" or "rm -rf /bin" etc.
        # But allow "rm -rf /home/aimeat/anvilos/..."
        if "rm -rf / " in cmd or cmd.strip() == "rm -rf /":
             return False, "VIOLATION: Destructive command (ROOT) rejected by Warden."
        
        # Check for other dangerous absolute paths if needed
        # For now, let's just make sure it's not "rm -rf /" followed by nothing or space.
        
        if "mkfs " in cmd and "/dev/sd" in cmd: # Block formatting real disks
             return False, "VIOLATION: Disk formatting rejected by Warden."

    elif op == "FILE_WRITE":
        path = pld.get("path", "")
        # 3. Protected Paths
        if "runtime/security" in path or "cortex.db" in path:
            return False, "VIOLATION: Write to protected system path rejected."
            
        # 4. The Collar (RFC-0058 / RFC-000666.2)
        # Any file written to 'src/' or 'native/' MUST be .anv or .json (MicroJSON sidecar)
        # Exemptions: logs, temporary build artifacts, documentation
        is_source_code = "src/" in path or "native/" in path
        is_exempt = "logs/" in path or "tmp/" in path or "build_artifacts/" in path or "DOCS/" in path
        
        if is_source_code and not is_exempt:
            valid_extensions = (".anv", ".json")
            if not path.endswith(valid_extensions):
                return False, f"VIOLATION (THE COLLAR): Write to {path} rejected. Only .anv or .json allowed for source code."
            
    return True, "OK"

def main_loop():
    logger.info("Processor Daemon Online. Monitoring card_stack...")
    DB.log_stream("MAINFRAME", "STARTUP", {"msg": "Processor Daemon Online"})
    
    while True:
        try:
            card = DB.pop_card()
            
            if card:
                card_id = card['id']
                op = card['op']
                
                try:
                    pld = json.loads(card['pld'])
                except json.JSONDecodeError as e:
                    logger.error(f"Card {card_id} Malformed Payload: {e}")
                    DB.log_result(card_id, {"error": "Malformed MicroJSON Payload"}, 9)
                    DB.log_stream("MAINFRAME", "ERROR", {"card": card_id, "error": str(e)})
                    continue
                
                logger.info(f"Processing Card {card_id}: {op}")
                DB.log_stream("MAINFRAME", "PROCESSING", {"card": card_id, "op": op})
                
                # --- WARDEN CHECK ---
                allowed, reason = _validate_card(op, pld)
                if not allowed:
                    logger.warning(f"Card {card_id} REJECTED by Warden: {reason}")
                    DB.log_result(card_id, {"error": reason}, 9) # 9 = Jam/Error
                    DB.log_stream("MAINFRAME", "REJECTED", {"card": card_id, "reason": reason})
                    continue

                result = None
                success = False
                
                if op == "SYS_CMD":
                    result = execute_sys_cmd(pld)
                    success = "error" not in result and result.get("exit_code") == 0
                elif op == "FILE_WRITE":
                    result = execute_file_write(pld)
                    success = "error" not in result
                elif op == "GEN_OP":
                    logger.info("Executing GEN_OP (No-op)")
                    result = {"msg": "GEN_OP executed (Simulation)"}
                    success = True
                else:
                    result = {"error": f"Unknown Operation: {op}"}
                    success = False
                
                # Mark card as PUNCHED (2) or JAMMED (9)
                stat = 2 if success else 9
                DB.log_result(card_id, result, stat)
                logger.info(f"Card {card_id} finished with status {stat}")
                DB.log_stream("MAINFRAME", "COMPLETE", {"card": card_id, "status": stat})
            
        except Exception as e:
            logger.error(f"Loop Error: {e}")
            DB.log_stream("MAINFRAME", "CRASH", {"error": str(e)})
            
        time.sleep(2)

if __name__ == "__main__":
    main_loop()
