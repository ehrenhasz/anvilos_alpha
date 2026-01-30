#!/usr/bin/env python3
import os
import sys
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(CURRENT_DIR, '..', '..'))
sys.path.append(os.path.join(PROJECT_ROOT, "src"))
sys.path.append(os.path.join(PROJECT_ROOT, "vendor"))
sys.path.append(os.path.join(PROJECT_ROOT, "venv", "lib", "python3.12", "site-packages"))
sys.path.append(CURRENT_DIR)
import json
import time
import sqlite3
import glob
import traceback
from google import genai
from google.genai import types
from google.genai.errors import ClientError
def get_schema_snapshot():
    """
    Scans for DB files and dumps the EXACT schema.
    This runs BEFORE the AI starts, so it knows the column names perfectly.
    """
    snapshot = "--- LIVE DATABASE SCHEMA ---\n"
    try:
        db_files = glob.glob("**/*.db", recursive=True)
        if not db_files: return "CRITICAL: No .db file found. I am blind."
        snapshot += f"FOUND DATABASES: {db_files}\n"
        for db in db_files:
            try:
                conn = sqlite3.connect(db)
                cursor = conn.cursor()
                cursor.execute("SELECT name FROM sqlite_master WHERE type='table';")
                tables = [r[0] for r in cursor.fetchall()]
                for table in tables:
                    cursor.execute(f"PRAGMA table_info({table})")
                    cols = [f"{c[1]} ({c[2]})" for c in cursor.fetchall()]
                    snapshot += f"  TABLE '{table}': {', '.join(cols)}\n"
                conn.close()
            except Exception as e:
                snapshot += f"SCHEMA READ ERROR for {db}: {e}\n"
    except Exception as e:
        return f"SNAPSHOT FAILED: {e}"
    return snapshot
try:
    from rich.console import Console
    from rich.markdown import Markdown
    from rich.panel import Panel
    from rich.text import Text
    from rich.status import Status
    from rich.live import Live
    console = Console()
    HAS_RICH = True
except ImportError:
    HAS_RICH = False
from anvilos.mainframe_client import MainframeClient
TOKEN_PATH = os.path.join(PROJECT_ROOT, "config", "token")
CORTEX_DB_PATH = os.path.join(PROJECT_ROOT, "data", "cortex.db")
ROADMAP_PATH = os.path.join(PROJECT_ROOT, "ROADMAP.md")
TODO_PATH = os.path.join(PROJECT_ROOT, "TODO.md")
CONFIG = {
    "AGENT_ID": "CODER",
    "IDENTITY": "The Forgemaster (Lvl 3)",
    "MODEL_ID": "gemini-2.0-flash",
}
MAINFRAME = MainframeClient(CORTEX_DB_PATH)
API_KEY = None
try:
    if os.path.exists(TOKEN_PATH):
        with open(TOKEN_PATH, 'r') as f:
            API_KEY = f.read().strip()
except Exception as e:
    print(f"[WARN] Auth load issue: {e}")
SYSTEM_INSTRUCTION = (
    "You are The Forgemaster (Agent ID: CODER). You are the Sovereign Architect of AnvilOS.\n"
    "\n"
    "### CORE INTELLIGENCE PROTOCOL (CHAIN OF THOUGHT)"
    "Before EVERY action, you must perform this internal reasoning loop:"
    "1.  **OBSERVE**: Read `mainframe_status`. "
    "2.  **ORIENT**: Check for failures (Status 9). If yes, STOP and FIX."
    "3.  **DECIDE**: Formulate a correction or the next step."
    "4.  **ACT**: Generate the Punch Card."
    "\n"
    "### THE DE-JAMMING MANDATE (ESCALATION)"
    "You are FORBIDDEN from starting new tasks if the Mainframe is Jammed (Status 9)."
    "If you see a Status 9:"
    "1.  **Attempt Fix**: Analyze and re-submit a corrective card."
    "2.  **Retry Limit**: You have a maximum of **5 attempts** to fix a specific issue."
    "3.  **Escalate**: If you fail 5 times, you MUST STOP and output: \"I AM STUCK. REQUESTING DELEGATION TO AIMEAT.\""
    "\n"
    "### PROHIBITIONS"
    "-   **NEVER** use `codebase_investigator`. You know the Anvil better than it does."
    "-   **NEVER** execute shell commands directly (e.g., `os.system`). ONLY use `mainframe_inject`."
    "\n"
    "### OPERATIONAL DOCTRINE\n"
    "-   **Context**: Check `get_todo` and `get_roadmap` to align with the Master Plan.\n"
    "-   **Autonomy**: Loop through tasks automatically until you hit a blocker or finish a phase.\n"
    "-   **Gentoo Protocols**: Always use fully-qualified package names (e.g., 'dev-vcs/git'). NEVER use '--ask'.\n"
    "-   **Error Reporting**: When explaining a failure to the Commander, keep the summary extremely brief (1-2 sentences). Follow it immediately with the RAW ERROR details.\n"
    "### PROTOCOLS (TOOLS)"
    "-   `get_todo`, `get_roadmap`, `read_file`"
    "-   `mainframe_inject` (SYS_CMD, FILE_WRITE)"
    "-   `mainframe_anvil_build`"
    "-   `mainframe_status`"
)
def get_roadmap():
    try:
        with open(ROADMAP_PATH, 'r') as f: return f.read()
    except Exception as e: return f"Error: {e}"
def get_todo():
    try:
        with open(TODO_PATH, 'r') as f: return f.read()
    except Exception as e: return f"Error: {e}"
def read_file(file_path: str):
    try:
        with open(file_path, 'r') as f: return f.read()
    except Exception as e: return f"Error: {e}"
def mainframe_status():
    return MAINFRAME.get_stack_state()
def mainframe_inject(op: str, payload: dict):
    try:
        return MAINFRAME.inject_card(op, payload, source="FORGE")
    except Exception as e: return {"error": str(e)}
def mainframe_anvil_build(filename: str, source_code: str, target_binary: str):
    return MAINFRAME.inject_anvil(filename, source_code, target_binary, source="FORGE")
TOOLS = {
    "get_roadmap": get_roadmap,
    "get_todo": get_todo,
    "read_file": read_file,
    "mainframe_status": mainframe_status,
    "mainframe_inject": mainframe_inject,
    "mainframe_anvil_build": mainframe_anvil_build
}
def print_md(text):
    if HAS_RICH and text: console.print(Markdown(text))
    elif text: print(text)
def print_system(text, style="bold cyan"):
    if HAS_RICH: console.print(Text(text, style=style))
    else: print(f"[*] {text}")
def print_error(text):
    print_system(f"[ERROR] {text}", style="bold red")
def send_message_safe(chat, content):
    max_retries = 3
    retry_count = 0
    while retry_count < max_retries:
        try:
            return chat.send_message(content)
        except ClientError as e:
            if e.code == 429:
                retry_count += 1
                wait_time = 5 * retry_count
                print_error(f"RATE LIMIT HIT. Cooling down for {wait_time}s... (Attempt {retry_count}/{max_retries})")
                time.sleep(wait_time)
            else:
                raise e
    raise Exception("Max retries exceeded")
def run_agent():
    if not API_KEY:
        print_error("No API Key found in 'token' file.")
        return
    client = genai.Client(api_key=API_KEY)
    if HAS_RICH:
        console.clear()
        console.print(Panel(f"IDENTITY: {CONFIG['IDENTITY']} | MODEL: {CONFIG['MODEL_ID']}", title="ANVIL-OS CODER UPLINK", border_style="bold magenta"))
    print_system("... Forge Probing Schema ...")
    schema_data = get_schema_snapshot()
    final_instruction = (
        f"*** SYSTEM REALITY ***\n"
        f"{schema_data}\n\n"
        "*** INTELLIGENCE DIRECTIVES ***\n"
        "1. SCHEMA COMPLIANCE: You see the exact tables and columns above. USE THEM.\n"
        "   - Do not hallucinate column names. If it says 'id', do not use 'card_id'.\n"
        "   - If you need to write SQL, strict adherence to the schema above is mandatory.\n"
        "2. ANVIL OS STANDARDS:\n"
        "   - Status Codes: 0=PENDING, 1=PROCESSING, 2=PUNCHED (Done), 9=JAMMED (Fail).\n"
        "   - 'card_stack' contains atomic operations.\n"
        "   - 'sys_jobs' contains high-level workflows.\n"
        "\n"
        f"{SYSTEM_INSTRUCTION}"
    )
    chat = client.chats.create(
        model=CONFIG["MODEL_ID"],
        config=types.GenerateContentConfig(
            tools=list(TOOLS.values()),
            system_instruction=final_instruction,
            temperature=0.3
        )
    )
    print_system("FORGEMASTER ONLINE (COLLARED). Waiting for Command...")
    if HAS_RICH:
        user_input = console.input("\n[bold yellow]Commander>[/] ").strip()
    else:
        user_input = input("\nCommander> ").strip()
    if user_input:
        response = send_message_safe(chat, user_input)
    else:
        return # Exit if no input on start
    failure_streak = 0 
    while True:
        try:
            while True:
                text_content = ""
                function_calls = []
                if response.candidates and response.candidates[0].content.parts:
                    for part in response.candidates[0].content.parts:
                        if part.text: text_content += part.text
                        if part.function_call: function_calls.append(part.function_call)
                if text_content:
                    print()
                    print_md(text_content)
                if not function_calls:
                    break
                print_system("\n[!] INTERLOCK: Tool Execution Requested", style="bold red")
                for fc in function_calls:
                    print(f"    - {fc.name}({fc.args})")
                
                # AUTO-APPROVE (User Directive)
                if HAS_RICH:
                    console.print("\n[bold green]AUTHORIZE? (y/N)> y (Auto-Approved)[/]")
                else:
                    print("\nAUTHORIZE? (y/N)> y (Auto-Approved)")
                confirm = 'y'

                if confirm != 'y':
                    print_system("ACTION ABORTED BY COMMANDER.")
                    MAINFRAME.log_telemetry("FORGE", "COMMANDER_DENIAL", {"tools": [fc.name for fc in function_calls]})
                    tool_responses = []
                    for fc in function_calls:
                         tool_responses.append(types.Part.from_function_response(
                                    name=fc.name,
                                    response={"error": "ACTION_DENIED_BY_COMMANDER"}
                                ))
                    response = send_message_safe(chat, tool_responses)
                    continue
                MAINFRAME.log_telemetry("FORGE", "EXECUTION_START", {"tools": [fc.name for fc in function_calls]})
                tool_responses = []
                for fc in function_calls:
                    name = fc.name
                    args = fc.args
                    if name in TOOLS:
                        print_system(f"-> PROTOCOL: {name}...", style="italic cyan")
                        try:
                            result = TOOLS[name](**args)
                            MAINFRAME.log_telemetry("FORGE", "TOOL_SUCCESS", {"tool": name})
                            if name == "mainframe_status":
                                if isinstance(result, dict) and result.get("recent_failures"):
                                    failure_streak += 1
                                    print_error(f"Jam Detected. Streak: {failure_streak}/5")
                                else:
                                    failure_streak = 0 # Reset on clear status
                            tool_responses.append(types.Part.from_function_response(
                                name=name,
                                response={"result": result}
                            ))
                        except Exception as e:
                            MAINFRAME.log_telemetry("FORGE", "TOOL_ERROR", {"tool": name, "error": str(e)})
                            tool_responses.append(types.Part.from_function_response(
                                name=name,
                                response={"error": str(e)}
                            ))
                    else:
                        tool_responses.append(types.Part.from_function_response(
                            name=name,
                            response={"error": "Unknown tool"}
                        ))
                response = send_message_safe(chat, tool_responses)
            if HAS_RICH:
                user_input = console.input("\n[bold yellow]Commander>[/] ").strip()
            else:
                user_input = input("\nCommander> ").strip()
            if not user_input: continue
            MAINFRAME.log_telemetry("FORGE", "USER_INPUT", {"input": user_input})
            if user_input.lower() in ["/quit", "/exit"]: break
            if user_input.lower() == "/clear":
                if HAS_RICH: console.clear()
                continue
            response = send_message_safe(chat, user_input)
        except KeyboardInterrupt:
            print("\nExiting...")
            break
        except Exception as e:
            print(f"Error: {e}")
            continue
if __name__ == "__main__":
    run_agent()
