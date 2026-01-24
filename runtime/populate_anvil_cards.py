import os
import json
import uuid
from datetime import datetime

OSS_DIR = "oss_sovereignty"
QUEUE_FILE = "/mnt/anvil_temp/card_queue.json"

def get_directories(path):
    return [d for d in os.listdir(path) if os.path.isdir(os.path.join(path, d))]

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
    dirs = get_directories(OSS_DIR)
    queue = load_queue()
    
    print(f"Found {len(dirs)} directories in {OSS_DIR}.")
    
    new_cards = []
    for d in dirs:
        # Filter out Linux Kernel
        if "Linux_Kernel" in d:
            print(f"Skipping {d} (Linux Kernel exclusion).")
            continue

        description = f"ANVIL_PKG: Recode '{d}' for Anvil Build System. 1. Create/Update 'oss_sovereignty/{d}/metadata.json' (Schema: id, name, role, type, sovereign). 2. Create 'oss_sovereignty/{d}/anvil.build.sh' that compiles the project (configure/make) and installs to a local './dist' folder."

        # Check for duplicates based on description prefix
        if any(f"Recode '{d}'" in c['description'] for c in queue):
            print(f"Skipping {d}, already in queue.")
            continue

        card = {
            "id": str(uuid.uuid4())[:8],
            "description": description,
            "status": "todo",
            "created_at": datetime.now().isoformat()
        }
        queue.append(card)
        new_cards.append(card)
        print(f"Added card for {d}")

    save_queue(queue)
    print(f"Successfully added {len(new_cards)} cards to the queue.")

if __name__ == "__main__":
    main()
