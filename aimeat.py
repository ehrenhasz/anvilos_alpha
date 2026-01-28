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
import uuid
import json
try:
    import forge_directives
except ImportError:
    forge_directives = None
PROJECT_ROOT = os.getcwd()
if os.path.exists(os.path.join(PROJECT_ROOT, "vendor")):
    sys.path.append(os.path.join(PROJECT_ROOT, "vendor"))
sys.path.append(os.path.join(PROJECT_ROOT, "src"))
from google import genai
from google.genai import types
from google.genai.errors import ClientError
TOKEN_PATH = os.path.join(PROJECT_ROOT, "config", "token")
MEMORY_FILE = os.path.join(PROJECT_ROOT, "data", "aimeat_memory.txt")
MODEL_ID = "gemini-2.0-flash"
def log_to_cortex(event_type: str, details: str):
    db_path = os.path.join(PROJECT_ROOT, "data", "cortex.db")
    try:
        with sqlite3.connect(db_path) as conn:
            conn.execute("CREATE TABLE IF NOT EXISTS live_stream (id INTEGER PRIMARY KEY AUTOINCREMENT, agent_id TEXT, event_type TEXT, details TEXT, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)")
            conn.execute("INSERT INTO live_stream (agent_id, event_type, details) VALUES (?, ?, ?)", ("AIMEAT", event_type, details))
    except Exception: pass
def get_schema_snapshot():
    db_path = os.path.join(PROJECT_ROOT, "data", "cortex.db")
    if not os.path.exists(db_path): return "No Database Found."
    try:
        with sqlite3.connect(db_path) as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT sql FROM sqlite_master WHERE type='table'")
            return "\n".join([row[0] for row in cursor.fetchall()])
    except Exception as e:
        return f"Schema Error: {e}"
STATE_FILE = os.path.join(PROJECT_ROOT, "data", "aimeat_state.json")

def get_state():
    if os.path.exists(STATE_FILE):
        try:
            with open(STATE_FILE, 'r') as f:
                return json.load(f)
        except: pass
    return {"index": 0}

def save_state(index):
    try:
        with open(STATE_FILE, 'w') as f:
            json.dump({"index": index}, f)
    except: pass

def send_to_forge(limit=None, reset=False):
    """Sends Forge Directives to the Processor (card_stack). Tracks progress."""
    if not forge_directives:
        print("[ERROR] forge_directives.py not found.")
        return
    
    # Reload module
    try: importlib.reload(forge_directives)
    except: pass
        
    db_path = os.path.join(PROJECT_ROOT, "data", "cortex.db")
    
    # State Management
    state = get_state()
    start_index = state["index"]
    
    if reset:
        start_index = 0
        print("[STATE] Cursor reset to 0.")
    
    # Check DB to see if we should auto-reset (optional heuristic)
    # If stack is empty and we are at 0, that's fine.
    # If stack is empty and we are at 1000, maybe user cleared deck?
    # For now, we trust the explicit 'reset' flag or the stored state.
    
    try:
        with sqlite3.connect(db_path) as conn:
            cursor = conn.cursor()
            
            # Check if stack is empty to offer helpful hint
            cursor.execute("SELECT COUNT(*) FROM card_stack")
            stack_depth = cursor.fetchone()[0]
            if stack_depth == 0 and start_index > 0 and not reset:
                 print(f"[NOTICE] The Mainframe stack is empty, but my local cursor is at {start_index}.")
                 print("         If you cleared the deck (F5), use 'reset' to start over.")
                 # We continue anyway, treating this as "resume"
            
            directives = forge_directives.PHASE2_DIRECTIVES
            if not isinstance(directives, list): directives = list(directives)
            
            total_available = len(directives)
            
            # Slice based on state
            if start_index >= total_available:
                print(f"[FORGE] All {total_available} cards have already been injected.")
                return

            if limit:
                end_index = min(start_index + limit, total_available)
                batch = directives[start_index:end_index]
                print(f"[FORGE] Injecting cards {start_index} to {end_index} (Limit: {limit}).")
            else:
                batch = directives[start_index:]
                end_index = total_available
                print(f"[FORGE] Injecting remaining cards {start_index} to {end_index}.")

            count = 0
            for card in batch:
                card_id = str(uuid.uuid4())
                seq = card.get("seq", 999)
                op = card.get("op", "unknown_op")
                pld = card.get("pld", {})
                pld["_source"] = "COMMANDER"
                
                cursor.execute("""
                    INSERT INTO card_stack (id, seq, op, pld, stat, timestamp)
                    VALUES (?, ?, ?, ?, 0, ?)
                """, (card_id, seq, op, json.dumps(pld), time.time()))
                count += 1
                if count % 100 == 0:
                    sys.stdout.write(f"\r[FORGE] Injected {count}...")
                    sys.stdout.flush()
                    time.sleep(0.01) 
            
            conn.commit()
            
            # Update State
            save_state(end_index)
            
            print(f"\n[FORGE] Successfully injected {count} directives.")
            log_to_cortex("FORGE_SUBMIT", f"Injected {count} directives (Index {start_index}-{end_index}).")
            
    except Exception as e:
        print(f"[FORGE ERROR] Failed to inject: {e}")
        log_to_cortex("FORGE_FAIL", str(e))
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
        return output if output else "Command executed silently."
    except Exception as e:
        return f"Execution Error: {e}"
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
        print("... Scanning Database Schema ...")
        schema_data = get_schema_snapshot()
        print(f"\033[1;30m{schema_data}\033[0m") # Show user what we found
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
                system_instruction=system_instruction
            )
        )
        print("\033[1;32m[AIMEAT ONLINE] Sovereign Interface Ready.\033[0m")
        log_to_cortex("SESSION_START", "Operator connected.")
        while True:
            try:
                user_input = input("\033[1;32m@aimeat>\033[0m ").strip()
            except KeyboardInterrupt: continue
            if not user_input: continue
            if user_input.lower() in ["exit", "quit"]: break
            if user_input.lower() in ["reset", "reset forge"]:
                send_to_forge(limit=0, reset=True)
                continue

            if user_input.lower().startswith("words"):
                parts = user_input.split()
                limit = None
                for p in parts:
                    if p.isdigit():
                        limit = int(p)
                        break
                if limit:
                    send_to_forge(limit=limit)
                    continue
                else:
                    print("[CMD ERROR] Usage: words <number>")
                    continue

            if user_input.lower().startswith("send"):
                parts = user_input.split()
                limit = None
                for p in parts:
                    if p.isdigit():
                        limit = int(p)
                        break
                if limit:
                    send_to_forge(limit=limit)
                    continue
                else:
                    print("[CMD ERROR] Could not parse number of cards.")
                    continue
            if user_input.lower() in ["start", "start cards", "forge"]:
                send_to_forge() # Resumes from last state by default
                continue
            
            if user_input.lower() in ["status", "stack", "list"]:
                db_path = os.path.join(PROJECT_ROOT, "data", "cortex.db")
                try:
                    with sqlite3.connect(db_path) as conn:
                        cursor = conn.cursor()
                        # Counts
                        cursor.execute("SELECT stat, COUNT(*) FROM card_stack GROUP BY stat")
                        counts = dict(cursor.fetchall())
                        print(f"\n[MAINFRAME STATUS]")
                        print(f"PENDING (0): {counts.get(0, 0)}")
                        print(f"PUNCHED (2): {counts.get(2, 0)}")
                        print(f"JAMMED  (9): {counts.get(9, 0)}")
                        print(f"DEAD   (99): {counts.get(99, 0)}")
                        
                        # Top Cards
                        print("\n[TOP 10 PENDING CARDS]")
                        cursor.execute("SELECT id, seq, op, pld FROM card_stack WHERE stat = 0 ORDER BY priority DESC, timestamp ASC LIMIT 10")
                        rows = cursor.fetchall()
                        if not rows:
                            print("(None)")
                        else:
                            for row in rows:
                                pld_preview = row[3][:60] + "..." if len(row[3]) > 60 else row[3]
                                print(f"- [{row[1]}] {row[2]}: {pld_preview}")
                        print("")
                except Exception as e:
                    print(f"[STATUS ERROR] {e}")
                continue

            prompt = user_input
            if "fix" in user_input.lower():
                 prompt += " (MODE: AUTONOMOUS. Use the schema above. Reset status to 0 to retry. Loop until done.)"
            max_retries = 3
            retry_count = 0
            while retry_count < max_retries:
                try:
                    response = chat.send_message(prompt)
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
