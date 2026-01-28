import json
import uuid
from datetime import datetime
import os

QUEUE_FILE = "/mnt/anvil_temp/card_queue.json"
PACKAGES = [
    {"dir": "sys_07_Btop", "name": "Btop"},
    {"dir": "sys_08_Aerc", "name": "Aerc"}
]

def load_queue():
    if not os.path.exists(QUEUE_FILE):
        return []
    with open(QUEUE_FILE, 'r') as f:
        try:
            return json.load(f)
        except json.JSONDecodeError:
            return []

def save_queue(queue):
    with open(QUEUE_FILE, 'w') as f:
        json.dump(queue, f, indent=2)

def main():
    queue = load_queue()
    new_cards = []

    for pkg in PACKAGES:
        d = pkg["dir"]
        
        # 1. SOVEREIGNTY_TASK
        sov_desc = f"SOVEREIGNTY_TASK: Refactor {d}. 1. Clear content of 'oss_sovereignty/{d}'. 2. Find official git repo for '{pkg['name']}'. 3. Clone repo into 'oss_sovereignty/{d}'. 4. Remove '.git' folder to disconnect."
        
        # 2. ANVIL_PKG TASK
        anvil_desc = f"ANVIL_PKG: Recode '{d}' for Anvil Build System. 1. Create/Update 'oss_sovereignty/{d}/metadata.json' (Schema: id, name, role, type, sovereign). 2. Create 'oss_sovereignty/{d}/anvil.build.sh' that compiles the project (configure/make) and installs to a local './dist' folder."

        # Add cards
        card1 = {
            "id": str(uuid.uuid4())[:8],
            "description": sov_desc,
            "status": "todo",
            "created_at": datetime.now().isoformat()
        }
        card2 = {
            "id": str(uuid.uuid4())[:8],
            "description": anvil_desc,
            "status": "todo",
            "created_at": datetime.now().isoformat()
        }
        
        queue.append(card1)
        queue.append(card2)
        new_cards.extend([card1, card2])
        print(f"Added cards for {d}")

    save_queue(queue)
    print(f"Successfully added {len(new_cards)} cards.")

if __name__ == "__main__":
    main()
