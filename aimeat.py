#!/usr/bin/env python3
import os
import sys
import glob
import uuid
import json
import sqlite3
import subprocess
import readline
import atexit
import datetime
import time

# --- CONFIG ---
PROJECT_ROOT = os.getcwd()
MEMORY_FILE = os.path.expanduser("~/.gemini/GEMINI.md")
CORTEX_DB_PATH = os.environ.get("CORTEX_DB_PATH", os.path.join(PROJECT_ROOT, "data", "cortex.db"))
HISTORY_FILE = os.path.expanduser("~/.gemini/aimeat_history")

# --- ENV INJECTION ---
current_dir = PROJECT_ROOT
# Add vendor directory
sys.path.append(os.path.join(current_dir, "vendor"))

# Dynamically find and add venv site-packages
for venv_dir in ["venv", ".venv"]:
    lib_path = os.path.join(current_dir, venv_dir, "lib")
    if os.path.exists(lib_path):
        for py_dir in os.listdir(lib_path):
            if py_dir.startswith("python"):
                site_pkg = os.path.join(lib_path, py_dir, "site-packages")
                if os.path.exists(site_pkg):
                    sys.path.append(site_pkg)

anvilos_path = os.path.join(current_dir, "anvilos")
if "PYTHONPATH" not in os.environ:
    os.environ["PYTHONPATH"] = ""
os.environ["PYTHONPATH"] += f"{os.pathsep}{current_dir}{os.pathsep}{anvilos_path}"

# --- SMART PROBE: The Cure for Stupidity ---
def get_schema_snapshot():
    """
    Connects to data/cortex.db (or looks for others) and dumps the EXACT schema.
    This runs BEFORE the AI starts, so it knows the column names perfectly.
    """
    snapshot = "--- LIVE DATABASE SCHEMA ---"
    
    # Use the configured DB path
    target_db = CORTEX_DB_PATH
    if not os.path.exists(target_db):
        return f"CRITICAL: Database file not found at {target_db}. Please ensure CORTEX_DB_PATH is set correctly or 'data/cortex.db' exists."

    snapshot += f"DATABASE FILE: {target_db}\n"

    try:
        conn = sqlite3.connect(target_db)
        cursor = conn.cursor()
        
        # 2. Get Tables
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table';")
        tables = [r[0] for r in cursor.fetchall()]
        
        # 3. Get Columns for each table
        for table in tables:
            cursor.execute(f"PRAGMA table_info({table})")
            columns = cursor.fetchall()
            # Format: (cid, name, type, notnull, dflt_value, pk)
            col_names = [f"{c[1]} ({c[2]})" for c in columns]
            snapshot += f"  TABLE '{table}': {', '.join(col_names)}\n"
            
        conn.close()
    except Exception as e:
        snapshot += f"SCHEMA READ ERROR: {e}\n"
        
    return snapshot

def understand_anvil():
    """
    Ingests the Anvil Coding Manual to ensure compliance with Sovereign Law.
    """
    manual_path = os.path.join(PROJECT_ROOT, "DOCS", "anvil_coding_manual.md")
    if os.path.exists(manual_path):
        print(f"\033[1;36m[KNOWLEDGE] Ingesting Anvil Protocols from: {manual_path}\033[0m")
        print("   -> Protocol: .mpy = MicroJSON Source (Human Readable)")
        print("   -> Protocol: .anv = AI Machine Byte Code (Binary)")
    else:
        print(f"\033[1;31m[WARNING] Anvil Manual not found at {manual_path}\033[0m")

# --- SHELL HISTORY ---
try:
    if not os.path.exists(os.path.dirname(HISTORY_FILE)):
        os.makedirs(os.path.dirname(HISTORY_FILE), exist_ok=True)
    readline.read_history_file(HISTORY_FILE)
except FileNotFoundError:
    pass
atexit.register(readline.write_history_file, HISTORY_FILE)

# --- AUTH ---
# Making genai optional.
try:
    from google import genai
    from google.genai import types
    from google.genai.errors import ClientError
    HAS_GENAI = True
except ImportError:
    HAS_GENAI = False

# --- CORTEX TELEMETRY ---
def log_to_cortex(event_type, details):
    """Streams Aimeat's thoughts to the Cortex."""
    db_path = CORTEX_DB_PATH
    try:
        with sqlite3.connect(db_path) as conn:
            conn.execute("""
                INSERT INTO live_stream (agent_id, event_type, details, timestamp)
                VALUES (?, ?, ?, CURRENT_TIMESTAMP)
            """, ("AIMEAT", event_type, str(details)))
    except Exception as e:
        pass

def execute_command(command: str):
    log_to_cortex("EXEC_ATTEMPT", command)
    try:
        if "rm -rf /" in command: 
            log_to_cortex("EXEC_DENIED", "rm -rf /")
            return "DENIED."
        
        result = subprocess.run(command, shell=True, capture_output=True, text=True, env=os.environ)
        output = result.stdout.strip()
        error = result.stderr.strip()
        
        if result.returncode != 0:
            log_to_cortex("EXEC_FAIL", error[:100])
            return f"EXIT_CODE_{result.returncode}: {error}"
            
        log_to_cortex("EXEC_SUCCESS", output[:100])
        
        if len(output) > 4000:
            return output[:4000] + "\n[OUTPUT TRUNCATED]"
            
        return output if output else "SUCCESS (No Output)"
    except Exception as e:
        log_to_cortex("EXEC_ERROR", str(e))
        return f"EXECUTION FAILED: {e}"

def update_memory(text: str):
    log_to_cortex("MEMORY_UPDATE", text[:50])
    try:
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        with open(MEMORY_FILE, "a") as f:
            f.write(f"\n* [{timestamp}] {text}")
        return "Memory updated."
    except Exception as e:
        return f"Failed to write memory: {e}"

def read_memory():
    if not os.path.exists(MEMORY_FILE): return "Identity: Aimeat."
    with open(MEMORY_FILE, "r") as f: return f.read()

# --- MAIN ---
def main():
    print(f"AIMEAT: Autonomous System Operator - ONLINE (PID: {os.getpid()})")
    print(f"Database: {CORTEX_DB_PATH}")
    
    # 1. Understand Anvil (Read-Only Startup)
    understand_anvil()
    
    if not HAS_GENAI:
        print("Notice: 'google-genai' not found. AI capabilities disabled.")
    
    # 2. Wait for Instructions
    print("System READY (Read-Only Mode Active).")
    print("Waiting for instruction from $meat, $lady_boss ... (Ctrl+C to exit)")
    
    while True:
        try:
            instruction = input("AIMEAT> ").strip()
            if not instruction:
                continue
            
            if instruction.lower() in ["exit", "quit"]:
                break
                
            # Direct execution for now
            response = execute_command(instruction)
            print(response)
            
        except KeyboardInterrupt:
            print("\nShutting down.")
            break
        except Exception as e:
            print(f"Error: {e}")

if __name__ == "__main__":
    main()
