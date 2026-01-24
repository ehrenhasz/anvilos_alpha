#!/usr/bin/env python3
import os
import sys
import sqlite3
import json
import subprocess
import time
import logging

# --- PATH RESOLUTION ---
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SYSTEM_DB = "/var/lib/anvilos/db/cortex.db"
sys.path.append(PROJECT_ROOT)

from runtime.cortex.db_interface import CortexDB

# --- LOGGING ---
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - [PROCESSOR] - %(levelname)s - %(message)s'
)
logger = logging.getLogger()

DB = CortexDB(SYSTEM_DB)

def execute_sys_cmd(pld):
    cmd = pld.get("cmd")
    timeout = pld.get("timeout", 60)
    
    # Pre-processing: if command uses /dev/loop0, try to find a free one
    if "/dev/loop0" in cmd:
        try:
            free_loop = subprocess.check_output("sudo losetup -f", shell=True, text=True).strip()
            cmd = cmd.replace("/dev/loop0", free_loop)
            logger.info(f"Remapped /dev/loop0 to {free_loop}")
        except Exception as e:
            logger.warn(f"Failed to find free loop device: {e}")

    logger.info(f"Executing System Command: {cmd}")
    try:
        res = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)
        return {
            "stdout": res.stdout,
            "stderr": res.stderr,
            "exit_code": res.returncode
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
        if "rm -rf /" in cmd or "mkfs" in cmd:
             return False, "VIOLATION: Destructive command rejected by Warden."

    elif op == "FILE_WRITE":
        path = pld.get("path", "")
        # 3. Protected Paths
        if "runtime/security" in path or "cortex.db" in path:
            return False, "VIOLATION: Write to protected system path rejected."
            
    return True, "OK"

def main_loop():
    logger.info("Processor Daemon Online. Monitoring card_stack...")
    
    while True:
        try:
            card = DB.pop_card()
            
            if card:
                card_id = card['id']
                op = card['op']
                pld = json.loads(card['pld'])
                
                logger.info(f"Processing Card {card_id}: {op}")
                
                # --- WARDEN CHECK ---
                allowed, reason = _validate_card(op, pld)
                if not allowed:
                    logger.warning(f"Card {card_id} REJECTED by Warden: {reason}")
                    DB.log_result(card_id, {"error": reason}, 9) # 9 = Jam/Error
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
                    # Placeholder for generative operations (e.g., calling back to Architect/Gemini)
                    # For now, we just log it as a success no-op
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
            
        except Exception as e:
            logger.error(f"Loop Error: {e}")
            
        time.sleep(2)

if __name__ == "__main__":
    main_loop()
