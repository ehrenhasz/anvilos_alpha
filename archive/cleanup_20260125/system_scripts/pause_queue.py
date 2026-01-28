import json

QUEUE_FILE = "runtime/card_queue.json"

try:
    with open(QUEUE_FILE, "r") as f:
        queue = json.load(f)
    
    count = 0
    for card in queue:
        if card["status"] in ["pending", "review", "in_progress"]:
            card["status"] = "paused"
            count += 1
    
    with open(QUEUE_FILE, "w") as f:
        json.dump(queue, f, indent=2)
    
    print(f"Successfully paused {count} tasks.")

except Exception as e:
    print(f"Error: {e}")
