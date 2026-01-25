#!/usr/bin/env python3
import os
import sys
import json
import time
import sqlite3
import traceback
from google import genai
from google.genai import types

# Try to import rich for pretty printing
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

# --- PATH RESOLUTION ---
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = CURRENT_DIR
sys.path.append(os.path.join(PROJECT_ROOT, "runtime"))

from mainframe_client import MainframeClient

# --- CONFIGURATION ---
TOKEN_PATH = os.path.join(CURRENT_DIR, "config", "token")
CORTEX_DB_PATH = "/var/lib/anvilos/db/cortex.db"
ROADMAP_PATH = os.path.join(CURRENT_DIR, "ROADMAP.md")
TODO_PATH = os.path.join(CURRENT_DIR, "TODO.md")

# --- SOVEREIGN CONFIG ---
CONFIG = {
    "AGENT_ID": "CODER",
    "IDENTITY": "The Forgemaster (Lvl 3)",
    "MODEL_ID": "gemini-2.0-flash",
}

MAINFRAME = MainframeClient(CORTEX_DB_PATH)

# --- AUTHENTICATION ---
API_KEY = None
try:
    if os.path.exists(TOKEN_PATH):
        with open(TOKEN_PATH, 'r') as f:
            API_KEY = f.read().strip()
except Exception as e:
    print(f"[WARN] Auth load issue: {e}")

# --- UNIFIED SYSTEM INSTRUCTION ---
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

# --- TOOLS ---

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
        return MAINFRAME.inject_card(op, payload)
    except Exception as e: return {"error": str(e)}

def mainframe_anvil_build(filename: str, source_code: str, target_binary: str):
    return MAINFRAME.inject_anvil(filename, source_code, target_binary)

TOOLS = {
    "get_roadmap": get_roadmap,
    "get_todo": get_todo,
    "read_file": read_file,
    "mainframe_status": mainframe_status,
    "mainframe_inject": mainframe_inject,
    "mainframe_anvil_build": mainframe_anvil_build
}

# --- UI HELPERS ---
def print_md(text):
    if HAS_RICH and text: console.print(Markdown(text))
    elif text: print(text)

def print_system(text, style="bold cyan"):
    if HAS_RICH: console.print(Text(text, style=style))
    else: print(f"[*] {text}")

def print_error(text):
    print_system(f"[ERROR] {text}", style="bold red")

# --- CORE: AUTO-LOOP ---
def run_agent():
    if not API_KEY:
        print_error("No API Key found in 'token' file.")
        return

    client = genai.Client(api_key=API_KEY)
    
    if HAS_RICH:
        console.clear()
        console.print(Panel(f"IDENTITY: {CONFIG['IDENTITY']} | MODEL: {CONFIG['MODEL_ID']}", title="ANVIL-OS CODER UPLINK", border_style="bold magenta"))
    
    chat = client.chats.create(
        model=CONFIG["MODEL_ID"],
        config=types.GenerateContentConfig(
            tools=list(TOOLS.values()),
            system_instruction=SYSTEM_INSTRUCTION,
            temperature=0.3
        )
    )

    print_system("FORGEMASTER ONLINE. Initiating Autonomous Startup Sequence...")

    # --- STARTUP: AUTO-FIX & LOOP ---
    initial_prompt = (
        "INITIATE SEQUENCE:\n"
        "1. Check mainframe_status."
        "2. If Jams (Status 9) exist, fix them immediately (Max 5 retries)."
        "3. If Clear, check TODO.md and execute the next task."
        "4. Loop this process until you need clarification or hit the retry limit."
    )
    
    response = chat.send_message(initial_prompt)

    failure_streak = 0 

    while True:
        try:
            # --- THE AUTONOMOUS LOOP ---
            while True:
                # 1. Extract content
                text_content = ""
                function_calls = []
                if response.candidates and response.candidates[0].content.parts:
                    for part in response.candidates[0].content.parts:
                        if part.text: text_content += part.text
                        if part.function_call: function_calls.append(part.function_call)

                # 2. Print Reasoning
                if text_content:
                    print()
                    print_md(text_content)
                    # Check for explicit surrender
                    if "I AM STUCK" in text_content or "REQUESTING DELEGATION" in text_content:
                        print_error("Agent Surrender Detected. Halting Loop.")
                        function_calls = [] # Prevent further tools
                        break 

                # 3. Check for Tool Calls
                if not function_calls:
                    break

                # 4. Execute Tools
                tool_responses = []
                for fc in function_calls:
                    name = fc.name
                    args = fc.args
                    
                    if name in TOOLS:
                        print_system(f"-> PROTOCOL: {name}...", style="italic cyan")
                        try:
                            result = TOOLS[name](**args)
                            
                            # Simple heuristic for failure tracking
                            if name == "mainframe_status":
                                if isinstance(result, dict) and result.get("recent_failures"):
                                    failure_streak += 1
                                    print_error(f"Jam Detected. Streak: {failure_streak}/5")
                                else:
                                    failure_streak = 0 # Reset on clear status
                            
                            if failure_streak >= 5:
                                tool_responses.append(types.Part.from_function_response(
                                    name=name,
                                    response={"error": "MAX_RETRIES_REACHED. YOU MUST DELEGATE TO AIMEAT."}
                                ))
                            else:
                                tool_responses.append(types.Part.from_function_response(
                                    name=name,
                                    response={"result": result}
                                ))

                        except Exception as e:
                            tool_responses.append(types.Part.from_function_response(
                                name=name,
                                response={"error": str(e)}
                            ))
                    else:
                        tool_responses.append(types.Part.from_function_response(
                            name=name,
                            response={"error": "Unknown tool"}
                        ))

                # 5. Feed back to model
                response = chat.send_message(tool_responses)
            
            # --- USER INTERRUPT ---
            if HAS_RICH:
                user_input = console.input("\n[bold yellow]Commander>[/] ").strip()
            else:
                user_input = input("\nCommander> ").strip()

            if not user_input: continue
            if user_input.lower() in ["/quit", "/exit"]: break
            if user_input.lower() == "/clear":
                if HAS_RICH: console.clear()
                continue

            response = chat.send_message(user_input)

        except Exception as e:
            print_error(f"CRITICAL FAULT: {e}")
            break

if __name__ == "__main__":
    run_agent()
