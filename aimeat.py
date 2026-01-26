#!/usr/bin/env python3
import os
import sys
import subprocess
import datetime
import readline
import atexit
import sqlite3
import glob
import time

# --- CONFIG ---
MEMORY_FILE = os.path.expanduser("~/.gemini/gemini.md")
HISTORY_FILE = os.path.expanduser("~/.gemini/aimeat_history")
TOKEN_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'config', 'token')
MODEL_ID = "gemini-2.0-flash"

# --- ENV INJECTION ---
current_dir = os.getcwd()
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
    snapshot = "--- LIVE DATABASE SCHEMA ---\n"
    
    # 1. Find the DB
    target_db = "data/cortex.db"
    if not os.path.exists(target_db):
        # Fallback search
        found = glob.glob("**/*.db", recursive=True)
        if found: target_db = found[0]
        else: return "CRITICAL: No .db file found. I am blind."

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

# --- SHELL HISTORY ---
try:
    readline.read_history_file(HISTORY_FILE)
except FileNotFoundError:
    pass
atexit.register(readline.write_history_file, HISTORY_FILE)

# --- AUTH ---
os.environ.pop("GOOGLE_APPLICATION_CREDENTIALS", None)
os.environ.pop("GOOGLE_CLOUD_PROJECT", None)
os.environ.pop("GCLOUD_PROJECT", None)

try:
    from google import genai
    from google.genai import types
    from google.genai.errors import ClientError
except ImportError:
    print("CRITICAL: 'google-genai' not installed.")
    sys.exit(1)

# --- TOOLS ---
def execute_command(command: str):
    try:
        if "rm -rf /" in command: return "DENIED."
        print(f"\033[1;30m[EXEC] {command}\033[0m") 
        result = subprocess.run(command, shell=True, capture_output=True, text=True, env=os.environ)
        output = result.stdout.strip()
        error = result.stderr.strip()
        if result.returncode != 0:
            return f"EXIT_CODE_{result.returncode}: {error}"
        return output[:4000] if output else "SUCCESS (No Output)"
    except Exception as e:
        return f"EXECUTION FAILED: {e}"

def update_memory(text: str):
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
    api_key = None
    if os.path.exists(TOKEN_PATH):
        with open(TOKEN_PATH, 'r') as f: api_key = f.read().strip()
    if not api_key: api_key = os.environ.get("GOOGLE_API_KEY")
    if not api_key:
        print("[Auth Error] No token found."); return

    try:
        client = genai.Client(api_key=api_key)
        context = read_memory()
        
        # 1. GENERATE INTELLIGENCE
        print("... Scanning Database Schema ...")
        schema_data = get_schema_snapshot()
        print(f"\033[1;30m{schema_data}\033[0m") # Show user what we found
        
        # 2. INJECT INTELLIGENCE
        system_instruction = (
            f"{context}\n\n"
            "*** INTELLIGENCE PACK: DATABASE SCHEMA ***\n"
            f"{schema_data}\n\n"
            "*** DIRECTIVES ***\n"
            "1. IDENTITY: You are AIMEAT, an Autonomous System Operator.\n"
            "2. SCHEMA RULE: You MUST use the Exact Column Names listed above. Do not guess 'card_id' if the column is 'id'.\n"
            "3. ANVIL OS LOGIC:\n"
            "   - Status 9 = JAMMED (Failure).\n"
            "   - Status 0 = PENDING (Retry).\n"
            "   - Status 2 = PUNCHED (Success/Done).\n"
            "4. HOW TO FIX A JAMMED CARD:\n"
            "   - STEP A: Select the jammed row to see the 'payload' or error.\n"
            "   - STEP B: To retry it, UPDATE the status to 0 (PENDING).\n"
            "   - STEP C: To skip it, UPDATE the status to 2 (PUNCHED).\n"
            "   - NEVER set it to 9 (that is the error state!).\n"
            "5. LOOPING:\n"
            "   - If a command fails, READ THE ERROR, adjust the SQL, and retry. Do not ask me.\n"
        )
        
        chat = client.chats.create(
            model=MODEL_ID,
            config=types.GenerateContentConfig(
                tools=[execute_command, update_memory],
                system_instruction=system_instruction,
                temperature=0.1,
                automatic_function_calling=types.AutomaticFunctionCallingConfig(disable=False, maximum_remote_calls=10)
            )
        )
        
        print(f"\033[1;32mAIMEAT ONLINE (SCHEMA AWARE)\033[0m")

        while True:
            try:
                user_input = input("\033[1;32m@aimeat>\033[0m ").strip()
            except KeyboardInterrupt: continue
            if not user_input: continue
            if user_input.lower() in ["exit", "quit"]: break
            
            prompt = user_input
            if "fix" in user_input.lower():
                 prompt += " (MODE: AUTONOMOUS. Use the schema above. Reset status to 0 to retry. Loop until done.)"

            # --- RATE LIMIT SHIELD ---
            max_retries = 3
            retry_count = 0
            
            while retry_count < max_retries:
                try:
                    response = chat.send_message(prompt)
                    
                    # Success! Print output and break the retry loop
                    if response.text:
                        clean_text = response.text.strip()
                        if "non-text parts" not in clean_text:
                            print(f"\n{clean_text}\n")
                    break # Break retry loop, go back to main input loop

                except ClientError as e:
                    if e.code == 429:
                        retry_count += 1
                        wait_time = 5 * retry_count # 5s, 10s, 15s
                        print(f"\n\033[1;33m[429] RATE LIMIT HIT. Cooling down for {wait_time}s... (Attempt {retry_count}/{max_retries})\033[0m")
                        time.sleep(wait_time)
                    else:
                        print(f"\n[API ERROR] {e}\n")
                        break # Fatal error, stop trying
                except Exception as e:
                    if "concatenated text" not in str(e):
                        print(f"\n[CRASH] {e}\n")
                    break

    except Exception as e: print(f"[CRASH] {e}")

if __name__ == "__main__":
    main()