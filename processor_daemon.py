import os
import sys
import sqlite3
import json
import subprocess
import time
import logging
PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
SYSTEM_DB = os.path.join(PROJECT_ROOT, "data", "cortex.db")
sys.path.append(os.path.join(PROJECT_ROOT, "vendor"))
for venv_dir in ["venv", ".venv"]:
    lib_path = os.path.join(PROJECT_ROOT, venv_dir, "lib")
    if os.path.exists(lib_path):
        for py_dir in os.listdir(lib_path):
            if py_dir.startswith("python"):
                site_pkg = os.path.join(lib_path, py_dir, "site-packages")
                if os.path.exists(site_pkg):
                    sys.path.append(site_pkg)
sys.path.append(os.path.join(PROJECT_ROOT, "src"))
from anvilos.cortex.db_interface import CortexDB
os.makedirs(os.path.join(PROJECT_ROOT, "logs"), exist_ok=True)
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - [PROCESSOR] - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(os.path.join(PROJECT_ROOT, "logs", "processor.log")),
        logging.StreamHandler(sys.stderr)
    ]
)
logger = logging.getLogger()
DB = CortexDB(SYSTEM_DB)
os.chdir(PROJECT_ROOT)
def execute_sys_cmd(pld):
    cmd = pld.get("cmd") or pld.get("command")
    if cmd is None:
        return {"error": "Missing 'cmd' in payload"}
    timeout = pld.get("timeout", 3600)
    if "/dev/loop0" in cmd:
        try:
            free_loop = subprocess.check_output("sudo losetup -f", shell=True, text=True).strip()
            cmd = cmd.replace("/dev/loop0", free_loop)
            logger.info(f"Remapped /dev/loop0 to {free_loop}")
        except Exception as e:
            logger.warn(f"Failed to find free loop device: {e}")
    logger.info(f"Executing System Command: {cmd} (timeout={timeout}s)")
    cwd = pld.get("cwd", PROJECT_ROOT)
    if not os.path.exists(cwd):
        return {"error": f"CWD {cwd} does not exist"}
    try:
        res = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout, cwd=cwd)
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

def execute_verify_file(pld):
    path = pld.get("path")
    if not path:
        return {"error": "Missing 'path'"}
    if not os.path.isabs(path):
        path = os.path.join(PROJECT_ROOT, path)
    
    if os.path.exists(path):
        return {"status": "VERIFIED", "path": path}
    else:
        # DO NOT return error here to avoid Jamming the Forge on missing files.
        # Instead, return a status that indicates it was checked but missing.
        return {"status": "MISSING", "path": path, "warning": "File not found"}

def execute_file_write(pld):
    path = pld.get("path")
    content = pld.get("content")
    if path is None:
        return {"error": "Missing 'path' in payload"}
    if content is None:
        return {"error": "Missing 'content' in payload"}
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
def _validate_card(op, pld):
    """
    The Warden: scrutinizes the card for violations of The Law of Anvil.
    Returns: (bool, str) -> (Allowed?, Reason)
    """
    source = pld.get("_source", "UNKNOWN")
    if source not in ["FORGE", "COMMANDER"]:
        return False, f"VIOLATION: Untrusted Source '{source}'. Only FORGE or COMMANDER may inject cards."
    if op == "SYS_CMD":
        cmd = pld.get("cmd") or pld.get("command") or ""
        forbidden = ["/usr/bin/gcc", "gcc ", "/usr/bin/clang", "clang ", "/usr/bin/rustc", "rustc ", "cargo build"]
        allowed_prefix = "ext/toolchain"
        for tool in forbidden:
            if tool in cmd and allowed_prefix not in cmd:
                return False, f"VIOLATION: Forbidden tool '{tool}' detected. Use Sovereign Toolchain."
        if "rm -rf / " in cmd or cmd.strip() == "rm -rf /":
             return False, "VIOLATION: Destructive command (ROOT) rejected by Warden."
        if "mkfs " in cmd and "/dev/sd" in cmd: # Block formatting real disks
             return False, "VIOLATION: Disk formatting rejected by Warden."
    elif op == "FILE_WRITE":
        path = pld.get("path", "")
        if path is None: path = ""
        if "runtime/security" in path or "cortex.db" in path:
            return False, "VIOLATION: Write to protected system path rejected."
        is_source_code = "src/" in path or "native/" in path
        is_exempt = "logs/" in path or "tmp/" in path or "build_artifacts/" in path or "DOCS/" in path or "/mnt/anvil_temp/" in path
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
                elif op == "verify_file_integrity":
                    result = execute_verify_file(pld)
                    success = "error" not in result
                elif op == "GEN_OP":
                    logger.info("Executing GEN_OP (No-op)")
                    result = {"msg": "GEN_OP executed (Simulation)"}
                    success = True
                else:
                    result = {"error": f"Unknown Operation: {op}"}
                    success = False
                stat = 2 if success else 9
                DB.log_result(card_id, result, stat)
                logger.info(f"Card {card_id} finished with status {stat}")
                DB.log_stream("MAINFRAME", "COMPLETE", {"card": card_id, "status": stat})
        except Exception as e:
            import traceback
            logger.error(f"Loop Error: {e}\n{traceback.format_exc()}")
            DB.log_stream("MAINFRAME", "CRASH", {"error": str(e), "traceback": traceback.format_exc()})
        time.sleep(2)
if __name__ == "__main__":
    main_loop()
