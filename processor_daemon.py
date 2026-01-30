import os
import sys
import sqlite3
import json
import subprocess
import time
import logging
import shutil

print("DAEMON RELOADED v2")

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
        logging.FileHandler("/tmp/processor_daemon.log", delay=False),
        logging.StreamHandler(sys.stderr)
    ],
    force=True
)
logger = logging.getLogger()
DB = CortexDB(SYSTEM_DB)
os.chdir(PROJECT_ROOT)

TOOLCHAIN_GCC = os.path.join(PROJECT_ROOT, "ext", "toolchain", "bin", "x86_64-unknown-linux-musl-gcc")
TOOLCHAIN_MPY = os.path.join(PROJECT_ROOT, "oss_sovereignty", "sys_09_Anvil", "source", "mpy-cross", "build", "mpy-cross")
ARTIFACTS_ROOT = os.path.join(PROJECT_ROOT, "build_artifacts")

def get_rel_path(src):
    # Strip common prefixes to get a clean relative path
    prefixes = ["oss_sovereignty/sources/", "oss_sovereignty/"]
    for p in prefixes:
        if src.startswith(p):
            return src[len(p):]
    return src

def get_actual_src(src_path):
    """
    Robustly finds the source file, checking:
    1. Exact path
    2. Path with 'sources/' removed (if present)
    3. Path with 'sources/' added (if missing)
    """
    if os.path.exists(src_path): return src_path
    
    # Try removing "/sources/"
    if "/sources/" in src_path:
        alt = src_path.replace("/sources/", "/")
        if os.path.exists(alt): return alt

    # Try adding "sources/" (Legacy check)
    if "oss_sovereignty/" in src_path and "/sources/" not in src_path:
        alt = src_path.replace("oss_sovereignty/", "oss_sovereignty/sources/")
        if os.path.exists(alt): return alt

    return src_path

def execute_install_asset(pld, type_dir="rootfs_stage"):
    src_raw = pld.get("src")
    if not src_raw: return {"error": "Missing src"}
    if not os.path.isabs(src_raw): src_raw = os.path.join(PROJECT_ROOT, src_raw)
    
    src = get_actual_src(src_raw)
    logger.info(f"DEBUG_SRC: {src}")
    rel = get_rel_path(pld.get("src"))
    dst = os.path.join(ARTIFACTS_ROOT, type_dir, rel)
    
    try:
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        # Handle symlinks specifically to avoid "No such file" if target is missing
        if os.path.islink(src):
            link_target = os.readlink(src)
            if os.path.exists(dst): os.remove(dst)
            os.symlink(link_target, dst)
            return {"status": "INSTALLED_LINK", "dst": dst}
            
        if os.path.exists(src):
            if os.path.isdir(src):
                os.makedirs(dst, exist_ok=True)
                return {"status": "INSTALLED_DIR", "dst": dst}
            shutil.copy2(src, dst)
            return {"status": "INSTALLED", "dst": dst}
        else:
            return {"error": f"Source missing: {src}"}
    except Exception as e:
        return {"error": str(e)}

def execute_forge_c_object(pld):
    src_raw = pld.get("src")
    if not src_raw: return {"error": "Missing src"}
    if not os.path.isabs(src_raw): src_raw = os.path.join(PROJECT_ROOT, src_raw)
    
    src = get_actual_src(src_raw)
    rel = get_rel_path(pld.get("src"))
    base, _ = os.path.splitext(rel)
    obj_rel = base + ".o"
    dst = os.path.join(ARTIFACTS_ROOT, "objects", obj_rel)
    
    if not os.path.exists(TOOLCHAIN_GCC):
        return {"error": f"Toolchain missing: {TOOLCHAIN_GCC}"}
    
    includes = []
    
    # Brute-force fix for util-linux dependencies (Moved to top)
    if "util-linux" in src:
        ul_root = os.path.join(PROJECT_ROOT, "oss_sovereignty", "util-linux-2.39.3")
        includes.append(f"-I{os.path.join(ul_root, 'libuuid', 'src')}")
        includes.append(f"-I{os.path.join(ul_root, 'libblkid', 'src')}")
        logger.info(f"APPLIED UTIL-LINUX FIX. Root: {ul_root}")

    # Dynamic include logic
    # Clean rel path to ensure we match directory structure
    # rel might be "oss_sovereignty/util-linux-2.39.3/..." or "sources/util-linux..."
    # We want to find the package root.
    
    pkg_marker = "oss_sovereignty"
    if pkg_marker in src:
        # Extract path parts after oss_sovereignty
        # e.g. /.../oss_sovereignty/util-linux-2.39.3/...
        try:
            split_src = src.split(os.sep)
            if pkg_marker in split_src:
                idx = split_src.index(pkg_marker)
                if idx + 1 < len(split_src):
                    pkg_name = split_src[idx+1] # util-linux-2.39.3 or sources
                    if pkg_name == "sources" and idx + 2 < len(split_src):
                        pkg_name = split_src[idx+2] # actual package name
                    
                    # Construct package root
                    # Try finding where this package lives
                    candidates = [
                        os.path.join(PROJECT_ROOT, "oss_sovereignty", pkg_name),
                        os.path.join(PROJECT_ROOT, "oss_sovereignty", "sources", pkg_name)
                    ]
                    
                    pkg_root = None
                    for c in candidates:
                        if os.path.isdir(c):
                            pkg_root = c
                            break
                    
                    with open("/tmp/processor_debug.log", "a") as f:
                        f.write(f"DEBUG: src={src}, pkg_name={pkg_name}, pkg_root={pkg_root}\n")

                    if pkg_root:
                        logger.info(f"Detected pkg_root: {pkg_root}")
                        # Add standard include
                        inc_dir = os.path.join(pkg_root, "include")
                        if os.path.exists(inc_dir): includes.append(f"-I{inc_dir}")
                        
                        # Add root include (often needed for config.h)
                        includes.append(f"-I{pkg_root}")
                        
                        # Auto-inject config.h if present
                        config_h = os.path.join(pkg_root, "config.h")
                        if os.path.exists(config_h):
                            logger.info(f"Auto-injecting config.h: {config_h}")
                            includes.append(f"-include")
                            includes.append(config_h)
                        else:
                            logger.warning(f"config.h NOT FOUND at {config_h}")
                        
                        # Special cases
                        if "libtirpc" in pkg_root:
                            tirpc = os.path.join(pkg_root, "tirpc")
                            if os.path.exists(tirpc): includes.append(f"-I{tirpc}")
                        
                        if "util-linux" in pkg_root:
                            uuid_inc = os.path.join(pkg_root, "libuuid", "src")
                            if os.path.exists(uuid_inc): includes.append(f"-I{uuid_inc}")
                            blkid_inc = os.path.join(pkg_root, "libblkid", "src")
                            if os.path.exists(blkid_inc): includes.append(f"-I{blkid_inc}")
                            logger.info(f"Util-Linux Includes: {includes}")
        except Exception as e:
            logger.error(f"Error calculating includes: {e}")

    logger.info(f"Forging {rel} with includes: {includes}")
    cmd = [TOOLCHAIN_GCC] + includes + ["-c", src, "-o", dst]
    
    try:
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode == 0:
            return {"status": "FORGED", "artifact": dst}
        else:
            return {"error": "Compilation Failed", "stderr": res.stderr, "cmd": " ".join(cmd)}
    except Exception as e:
        return {"error": str(e)}

def execute_forge_py_artifact(pld):
    src_raw = pld.get("src")
    if not src_raw: return {"error": "Missing src"}
    if not os.path.isabs(src_raw): src_raw = os.path.join(PROJECT_ROOT, src_raw)
    
    src = get_actual_src(src_raw)
    rel = get_rel_path(pld.get("src"))
    # Change extension to .mpy
    base, _ = os.path.splitext(rel)
    mpy_rel = base + ".mpy"
    dst = os.path.join(ARTIFACTS_ROOT, "objects", mpy_rel)
    
    # Try multiple possible mpy-cross locations
    # FORCE STATIC BUILD PATH
    mpy_locations = [
        os.path.join(PROJECT_ROOT, "oss_sovereignty", "sys_09_Anvil", "source", "mpy-cross", "build", "mpy-cross"),
    ]
    actual_mpy = None
    for loc in mpy_locations:
        if os.path.exists(loc):
            actual_mpy = loc
            break

    if not actual_mpy:
        logger.error(f"MPY-Cross not found. Checked: {mpy_locations}")
        return {"error": f"MPY-Cross missing. Checked: {mpy_locations}"}
        
    logger.info(f"Using mpy-cross at: {actual_mpy}")
    cmd = [actual_mpy, src, "-o", dst]
    
    try:
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode == 0:
            return {"status": "TRANSMUTED", "artifact": dst}
        else:
            return {"error": "Transmutation Failed", "stderr": res.stderr}
    except Exception as e:
        return {"error": str(e)}

# --- SYS CMD (unchanged) ---
def execute_sys_cmd(pld):
    # Security already checked by validate_card
    cmd = pld.get("cmd") or pld.get("command")
    cwd = pld.get("cwd") or PROJECT_ROOT
    try:
        res = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True)
        return {"exit_code": res.returncode, "stdout": res.stdout, "stderr": res.stderr}
    except Exception as e:
        return {"error": str(e)}

def execute_file_write(pld):
    path = pld.get("path")
    content = pld.get("content")
    if not path or content is None: return {"error": "Missing path or content"}
    if not os.path.isabs(path): path = os.path.join(PROJECT_ROOT, path)
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(content)
        return {"status": "WRITTEN", "path": path}
    except Exception as e:
        return {"error": str(e)}

def execute_verify_file(pld):
    path = pld.get("path")
    if not path: return {"error": "Missing path"}
    if not os.path.isabs(path): path = os.path.join(PROJECT_ROOT, path)
    
    # Use robust search
    final_path = get_actual_src(path)
    
    if os.path.exists(final_path):
        return {"status": "VERIFIED", "path": final_path}
    else:
        return {"error": f"MISSING: {path} (checked {final_path})"}

def _validate_card(op, pld):
    """
    The Warden: scrutinizes the card for violations of The Law of Anvil.
    Returns: (bool, str) -> (Allowed?, Reason)
    """
    source = pld.get("_source", "UNKNOWN")
    if source not in ["FORGE", "COMMANDER", "FORGE_REPAIR"]:
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
                
                # --- Operation Handlers ---
                if op == "SYS_CMD":
                    result = execute_sys_cmd(pld)
                    success = "error" not in result and result.get("exit_code") == 0
                elif op == "FILE_WRITE":
                    result = execute_file_write(pld)
                    success = "error" not in result
                elif op == "verify_file_integrity":
                    result = execute_verify_file(pld)
                    success = "error" not in result
                elif op == "install_static_asset":
                    result = execute_install_asset(pld, "rootfs_stage")
                    success = "error" not in result
                elif op == "install_header":
                    result = execute_install_asset(pld, "rootfs_stage") # Headers go to rootfs too usually
                    success = "error" not in result
                elif op == "forge_c_object":
                    result = execute_forge_c_object(pld)
                    success = "error" not in result
                elif op == "forge_py_artifact":
                    result = execute_forge_py_artifact(pld)
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
        time.sleep(0.001)

if __name__ == "__main__":
    main_loop()
