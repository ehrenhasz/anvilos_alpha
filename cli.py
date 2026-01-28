import os
import sys
import subprocess
import readline
import atexit
HISTORY_FILE = os.path.expanduser("~/.gemini/cli_history")
try:
    if os.path.exists(HISTORY_FILE):
        readline.read_history_file(HISTORY_FILE)
except (FileNotFoundError, PermissionError):
    pass
atexit.register(readline.write_history_file, HISTORY_FILE)
def main():
    os.system('cls' if os.name == 'nt' else 'clear')
    print("ANVIL OS [Restricted Shell]")
    print("Type '@aimeat' to summon the agent.")
    while True:
        try:
            user_input = input("\033[1;30m$ \033[0m").strip()
            if not user_input: continue
            if user_input.startswith("@aimeat"):
                if os.path.exists("aimeat.py"):
                    subprocess.run([sys.executable, "aimeat.py"])
                else:
                    print("Error: aimeat.py not found.")
                print("\n[Agent Detached]")
                continue
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