#!/usr/bin/env python3

### PRIMARY BOOT SEQUENCE ###
import os
import sys
import json
import time
import subprocess

# --- PATH RESOLUTION ---
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = CURRENT_DIR
sys.path.append(os.path.join(PROJECT_ROOT, "src"))
sys.path.append(os.path.join(PROJECT_ROOT, "vendor"))

from google import genai
from google.genai import types

# Try to import rich
try:
    from rich.console import Console
    from rich.markdown import Markdown
    from rich.panel import Panel
    from rich.text import Text
    console = Console()
    HAS_RICH = True
except ImportError:
    HAS_RICH = False

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

# --- AUTHENTICATION ---
API_KEY = None
try:
    if os.path.exists(TOKEN_PATH):
        with open(TOKEN_PATH, 'r') as f:
            API_KEY = f.read().strip()
except Exception as e:
    print(f"[WARN] Auth load issue: {e}")

MAINFRAME = MainframeClient(CORTEX_DB_PATH)

# --- UNIFIED SYSTEM INSTRUCTION ---
SYSTEM_INSTRUCTION = "AIMEAT: ON BOOT, WAIT FOR COMMAND."

# --- WRAPPERS ---
def mainframe_status():
    return MAINFRAME.get_stack_state()

def mainframe_inject(op: str, payload: dict):
    # Operator has high priority
    return MAINFRAME.inject_card(op, payload) 

def mainframe_anvil_build(filename: str, source_code: str, target_binary: str):
    return MAINFRAME.inject_anvil(filename, source_code, target_binary)

TOOLS = {
    "mainframe_status": mainframe_status,
    "mainframe_inject": mainframe_inject,
    "mainframe_anvil_build": mainframe_anvil_build
}

# --- CLI ENTRY ---
def main():
    if len(sys.argv) > 1:
        cmd = sys.argv[1]
        if cmd == "status":
            print(json.dumps(mainframe_status(), indent=2))
        else:
            print(f"Unknown command: {cmd}")
    else:
        print("No command provided. Available commands: status.")

if __name__ == "__main__":
    main()