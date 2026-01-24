
import os
import sys
import time
import subprocess
import psutil

# Fix path to import from runtime root
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from collar import TheCollar

# Config
AGENT_ID = "dnd_dm"
DTRPG_EXE_PATH = "/home/aimeat/.wine/drive_c/Program Files/DriveThruRPG/DriveThruRPG.exe"
DTRPG_DIR = os.path.dirname(DTRPG_EXE_PATH)
LOG_FILE = "/home/aimeat/github/dnd_dm/dtrpg.log"

COLLAR = TheCollar(AGENT_ID)

def is_running(process_name):
    for proc in psutil.process_iter(['pid', 'name', 'cmdline']):
        try:
            # Check for the Windows executable name in the command line (Wine behavior)
            cmdline = proc.info['cmdline']
            if cmdline and any(process_name in part for part in cmdline):
                return proc
        except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
            pass
    return None

def launch_dtrpg():
    """
    Launches the DriveThruRPG client via Wine, wrapped in The Collar.
    """
    proc = is_running("DriveThruRPG.exe")
    if proc:
        COLLAR.log("DTRPG_CHECK", "launch_dtrpg", True, {"status": "ALREADY_RUNNING", "pid": proc.info['pid']})
        return

    COLLAR.log("DTRPG_START", "launch_dtrpg", True, {"status": "ATTEMPTING_LAUNCH"})
    
    # We use nohup and redirect output to manage the GnuTLS noise
    cmd = f'cd "{DTRPG_DIR}" && nohup wine "DriveThruRPG.exe" > "{LOG_FILE}" 2>&1 &'
    
    # Using COLLAR.sh to execute the shell command
    # Note: Because of '&', COLLAR.sh will return immediately with the shell status, not the app status.
    result = COLLAR.sh(cmd, context="DTRPG_LAUNCH_CMD")
    
    if result.returncode == 0:
        # Give it a moment to spawn
        time.sleep(3)
        proc = is_running("DriveThruRPG.exe")
        if proc:
             COLLAR.log("DTRPG_STATUS", "launch_dtrpg", True, {"status": "LAUNCH_SUCCESS", "pid": proc.info['pid']})
        else:
             COLLAR.log("DTRPG_STATUS", "launch_dtrpg", False, {"status": "LAUNCH_FAILED_NO_PID"})
    else:
        COLLAR.log("DTRPG_STATUS", "launch_dtrpg", False, {"status": "SHELL_EXEC_FAILED", "stderr": result.stderr})

def monitor_resources():
    """
    Checks CPU/Memory of the process.
    """
    proc = is_running("DriveThruRPG.exe")
    if not proc:
        return
        
    try:
        cpu = proc.cpu_percent(interval=1.0)
        mem = proc.memory_info().rss / (1024 * 1024) # MB
        
        COLLAR.log("DTRPG_METRICS", "monitor", True, {"cpu_percent": cpu, "memory_mb": round(mem, 2)})
    except Exception as e:
        COLLAR.log("DTRPG_METRICS", "monitor", False, {"error": str(e)})

if __name__ == "__main__":
    os.system('cls' if os.name == 'nt' else 'clear')
    if len(sys.argv) > 1 and "monitor" in sys.argv[1]:
        print(f"[DTRPG] Entering Monitoring Loop... (Ctrl+C to exit)")
        try:
            while True:
                monitor_resources()
                # Optional: Get the latest metrics from collar log for display
                proc = is_running("DriveThruRPG.exe")
                if proc:
                    try:
                        cpu = proc.cpu_percent(interval=None)
                        mem = proc.memory_info().rss / (1024 * 1024)
                        print(f"\r[DTRPG] PID: {proc.info['pid']} | CPU: {cpu:6.1f}% | MEM: {mem:8.2f} MB", end="", flush=True)
                    except: pass
                else:
                    print(f"\r[DTRPG] NOT RUNNING", end="", flush=True)
                time.sleep(2)
        except KeyboardInterrupt:
            print(f"\n[DTRPG] Monitoring stopped.")
    else:
        print(f"[DTRPG] Launching...")
        launch_dtrpg()
