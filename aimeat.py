#!/usr/bin/env python3

### PRIMARY BOOT SEQUENCE ###
import os
import sys
import json
import time
import subprocess
import sqlite3

# --- PATH RESOLUTION ---
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = CURRENT_DIR
sys.path.append(os.path.join(PROJECT_ROOT, "src"))
sys.path.append(os.path.join(PROJECT_ROOT, "vendor"))

# Import Shared Tools
from anvilos.mainframe_client import MainframeClient

# --- CONFIGURATION ---
TOKEN_PATH = os.path.join(CURRENT_DIR, "config", "token")
CORTEX_DB_PATH = os.path.join(CURRENT_DIR, "data", "cortex.db")

# --- SOVEREIGN CONFIG ---
CONFIG = {
    "AGENT_ID": "AIMEAT",
    "IDENTITY": "The Operator (Lvl 3)",
    "MODEL_ID": "gemini-2.0-flash",
}

MAINFRAME = MainframeClient(CORTEX_DB_PATH)

def get_daemon_status():
    """Checks if Architect and Processor daemons are running."""
    try:
        ps = subprocess.check_output(["ps", "aux"], text=True)
        return {
            "architect": "architect_daemon.py" in ps,
            "processor": "processor_daemon.py" in ps
        }
    except:
        return {"architect": False, "processor": False}

def report_for_duty():
    """Tactical system summary for the Operator."""
    daemons = get_daemon_status()
    stack = MAINFRAME.get_stack_state()
    
    # Check for latest chat history
    last_msg = "NONE"
    try:
        with sqlite3.connect(CORTEX_DB_PATH) as conn:
            row = conn.execute("SELECT message FROM chat_inbox ORDER BY timestamp DESC LIMIT 1").fetchone()
            if row: last_msg = row[0]
    except: pass

    print(f"\n[bold]AIMEAT SYSTEM ONLINE[/bold]")
    print(f"IDENTITY: {CONFIG['IDENTITY']}")
    print(f"CORTEX:   {'CONNECTED' if os.path.exists(CORTEX_DB_PATH) else 'OFFLINE'}")
    print(f"DAEMONS:  ARCHITECT=[{'ON' if daemons['architect'] else 'OFF'}] PROCESSOR=[{'ON' if daemons['processor'] else 'OFF'}]")
    print(f"STACK:    PENDING={stack.get('pending_count', 0)} PROCESSING={stack.get('processing_count', 0)}")
    print(f"LAST_IN:  {last_msg[:50]}...")
    print(f"\nREADY FOR COMMAND.\n")

# --- CLI ENTRY ---
def main():
    if len(sys.argv) > 1:
        cmd = sys.argv[1]
        if cmd == "status":
            print(json.dumps(MAINFRAME.get_stack_state(), indent=2))
        elif cmd == "report":
            report_for_duty()
        else:
            print(f"Unknown command: {cmd}")
    else:
        report_for_duty()

if __name__ == "__main__":
    # Minimal 'rich' wrapper for fast boot
    try:
        from rich import print
    except ImportError:
        pass 
    main()
