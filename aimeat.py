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
try:
    import forge_directives
except ImportError:
    forge_directives = None

# --- CONFIG ---
PROJECT_ROOT = os.getcwd()
MEMORY_FILE = os.path.expanduser("~/.gemini/gemini.md")
HISTORY_FILE = os.path.expanduser("~/.gemini/aimeat_history")
TOKEN_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'config', 'token')
MODEL_ID = "gemini-2.0-flash"

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

# --- CORTEX TELEMETRY ---
def log_to_cortex(event_type, details):
    """Streams Aimeat's thoughts to the Cortex."""
    db_path = os.path.join(PROJECT_ROOT, "data", "cortex.db")
    try:
        with sqlite3.connect(db_path) as conn:
            conn.execute("""
                INSERT INTO live_stream (agent_id, event_type, details, timestamp)
                VALUES (?, ?, ?, CURRENT_TIMESTAMP)
            """, ("AIMEAT", event_type, str(details)))
    except Exception: pass # Silent fail if DB locked/missing

def inject_directives():
    """Injects Forge Directives into the Mainframe card stack."""
    if not forge_directives:
        print("[ERROR] forge_directives.py not found.")
        return

    db_path = os.path.join(PROJECT_ROOT, "data", "cortex.db")
    print(f"[FORGE] Connecting to {db_path}...")
    
    try:
        with sqlite3.connect(db_path) as conn:
            cursor = conn.cursor()
            
            # Clear existing pending cards? No, append.
            count = 0
            for card in forge_directives.FULL_STACK:
                card_id = str(uuid.uuid4())
                seq = card.get("seq", 999)
                op = card.get("op", "unknown_op")
                
                # Ensure payload includes trust signature
                pld_dict = card.get("pld", {})
                pld_dict["_source"] = "COMMANDER"
                pld = json.dumps(pld_dict)
                
                # Check if exists to avoid dupes?
                # For now, just insert.
                cursor.execute("""
                    INSERT INTO card_stack (id, seq, op, pld, stat, agent_id, timestamp)
                    VALUES (?, ?, ?, ?, 0, 'AIMEAT', CURRENT_TIMESTAMP)
                """, (card_id, seq, op, pld))
                count += 1
            
            conn.commit()
            print(f"[FORGE] Successfully injected {count} cards into the Mainframe.")
            log_to_cortex("INJECT_CARDS", f"Injected {count} directives from forge.")

    except Exception as e:
        print(f"[FORGE ERROR] Failed to inject cards: {e}")
        log_to_cortex("INJECT_FAIL", str(e))

# --- TOOLS ---
def execute_command(command: str):
    log_to_cortex("EXEC_ATTEMPT", command)
    try:
        if "rm -rf /" in command: 
            log_to_cortex("EXEC_DENIED", "rm -rf /")
            return "DENIED."
        print(f"\033[1;30m[EXEC] {command}\033[0m") 
        result = subprocess.run(command, shell=True, capture_output=True, text=True, env=os.environ)
        output = result.stdout.strip()
        error = result.stderr.strip()
        if result.returncode != 0:
            log_to_cortex("EXEC_FAIL", error[:100])
            return f"EXIT_CODE_{result.returncode}: {error}"
        log_to_cortex("EXEC_SUCCESS", output[:100])
        return output[:4000] if output else "SUCCESS (No Output)"
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
            "4. CORTEX STREAMING: Your actions are now logged to the 'live_stream' table. Do not try to hide.\n"
            "5. THE LAW (RFC-000666.2): HOST CONTAMINATION IS FORBIDDEN.\n"
            "   - You MUST NOT use /usr/bin/cc or host GCC.\n"
            "   - You MUST use the Sovereign Toolchain (x86_64-bicameral...).\n"
            "6. THE COLLAR: The Mainframe now verifies card sources.\n"
            "   - You cannot inject cards directly unless you sign them (which you can't yet).\n"
            "   - You must work THROUGH the Forge or CLI.\n"
            "7. LOOPING:\n"
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
            
            # --- LOCAL OVERRIDES ---
            if user_input.lower() in ["start", "start cards", "forge"]:
                inject_directives()
                continue

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