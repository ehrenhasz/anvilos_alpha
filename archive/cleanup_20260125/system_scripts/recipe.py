import json
import os
import subprocess
from datetime import datetime
CARD_PATH = "runtime/card_queue.json"
CARD_ID = "sys_fix_failed_sov_paths"
def add_card():
    failed_commands = []
    if os.path.exists(CARD_PATH):
        with open(CARD_PATH, "r") as f:
            data = json.load(f)
            for card in data:
                if card.get("status") == "failed" and "assimilate.py" in card.get("command", ""):
                    cmd = card["command"]
                    failed_commands.append(cmd)
    if not failed_commands:
        print(">> No failed assimilation cards found.")
        return
    setup_cmd = "mkdir -p oss_sovereignty/sys_18_64Bit_Cloud/04_Languages_Runtimes oss_sovereignty/sys_18_64Bit_Cloud/03_Virtualization oss_sovereignty/sys_18_64Bit_Cloud/01_Operating_Systems oss_sovereignty/sys_17_32Bit_Era/01_Consoles oss_sovereignty/sys_17_32Bit_Era/02_Computers oss_sovereignty/sys_17_32Bit_Era/03_Toolchains"
    card = {
        "id": CARD_ID,
        "description": "FIX: Create missing parent directories for failed sovereignty cards and retry assimilation.",
        "status": "pending",
        "command": setup_cmd,
        "created_at": datetime.utcnow().isoformat() + "Z"
    }
    with open(CARD_PATH, "r") as f:
        queue = json.load(f)
    for c in queue:
        if c.get("status") == "failed" and "assimilate.py" in c.get("command", ""):
            c["status"] = "pending"
            c["result"] = "Reset for retry after directory fix."
    queue.insert(0, card)
    with open(CARD_PATH, "w") as f:
        json.dump(queue, f, indent=2)
    print(f">> Card {CARD_ID} added. Reset failed cards to pending.")
if __name__ == "__main__":
    add_card()
