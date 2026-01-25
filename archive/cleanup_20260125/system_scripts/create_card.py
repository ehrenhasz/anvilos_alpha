import json
import sys
import os
from datetime import datetime

QUEUE_FILE = "runtime/card_queue.json"

def create_card(task_id, description, command):
    if os.path.exists(QUEUE_FILE):
        try:
            with open(QUEUE_FILE, "r") as f:
                queue = json.load(f)
        except json.JSONDecodeError:
            queue = []
    else:
        queue = []

    new_card = {
        "id": task_id,
        "description": description,
        "status": "pending",
        "command": command,
        "created_at": datetime.utcnow().isoformat() + "Z"
    }

    queue.insert(0, new_card)

    with open(QUEUE_FILE, "w") as f:
        json.dump(queue, f, indent=2)
    
    print(f"Card {task_id} created.")

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: python3 create_card.py <ID> <DESC> <CMD>")
        sys.exit(1)
    create_card(sys.argv[1], sys.argv[2], sys.argv[3])
