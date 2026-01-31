#!/usr/bin/env python3
import os
import sys
import subprocess
import readline
import atexit

# --- CONFIG ---
HISTORY_FILE = os.path.expanduser("~/.gemini/cli_history")

# --- SHELL HISTORY SETUP ---
try:
    if os.path.exists(HISTORY_FILE):
        readline.read_history_file(HISTORY_FILE)
except (FileNotFoundError, PermissionError):
    pass
atexit.register(readline.write_history_file, HISTORY_FILE)

def main():
    # Clear screen on boot
    os.system('cls' if os.name == 'nt' else 'clear')
    print("ANVIL OS [Restricted Shell]")
    print("Type '@aimeat' to summon the agent.")

    while True:
        try:
            # The "Dumb" Prompt (now with readline history support)
            user_input = input("\033[1;30m$ \033[0m").strip()
            
            if not user_input: continue
            
            # --- THE TRIGGER ---
            if user_input.startswith("@aimeat"):
                # Spawn agent. Force usage of the current Python interpreter.
                if os.path.exists("aimeat.py"):
                    subprocess.run([sys.executable, "aimeat.py"])
                else:
                    print("Error: aimeat.py not found.")
                
                # When agent exits, we are back here
                print("\n[Agent Detached]")
                continue

            # Standard exit
            if user_input in ["exit", "quit"]:
                sys.exit(0)

            print(f"Unknown command: {user_input}")

        except KeyboardInterrupt:
            print("\n")
            continue
        except EOFError:
            print("\nExiting.")
            sys.exit(0)

if __name__ == "__main__":
    main()