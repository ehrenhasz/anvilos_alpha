import os
import json
import math
from datetime import datetime

SOV_DIR = "oss_sovereignty"
QUEUE_FILE = "runtime/card_queue.json"
BATCH_SIZE = 5

def generate_batches():
    targets = []
    
    # 1. Get all top-level directories
    all_items = os.listdir(SOV_DIR)
    top_dirs = [d for d in all_items if os.path.isdir(os.path.join(SOV_DIR, d))]
    top_dirs.sort()
    
    for d in top_dirs:
        full_path = os.path.join(SOV_DIR, d)
        
        if d == "sys_99_Legacy_Bin":
            # Recurse into sys_99_Legacy_Bin (Category/App)
            # We want depth 2 subdirs: sys_99_Legacy_Bin/CATEGORY/APP
            categories = [c for c in os.listdir(full_path) if os.path.isdir(os.path.join(full_path, c))]
            categories.sort()
            for cat in categories:
                cat_path = os.path.join(full_path, cat)
                apps = [a for a in os.listdir(cat_path) if os.path.isdir(os.path.join(cat_path, a))]
                apps.sort()
                for app in apps:
                    # Target: oss_sovereignty/sys_99_Legacy_Bin/CATEGORY/APP
                    targets.append(os.path.join("sys_99_Legacy_Bin", cat, app))
        else:
            # Standard system folder
            targets.append(d)
            
    print(f">> Found {len(targets)} sovereignty targets.")
    
    # 3. Chunk into batches
    batches = [targets[i:i + BATCH_SIZE] for i in range(0, len(targets), BATCH_SIZE)]
    
    new_cards = []
    
    for i, batch in enumerate(batches):
        batch_id = f"sov_batch_run_{datetime.now().strftime('%Y%m%d')}_{i+1:03d}"
        
        # Construct the chained command
        commands = []
        for folder in batch:
            full_path = os.path.join(SOV_DIR, folder)
            commands.append(f"bash system/run_sovereignty_task.sh {full_path}")
        
        full_command = " && ".join(commands)
        
        card = {
            "id": batch_id,
            "description": f"SOV BATCH {i+1}: {', '.join([os.path.basename(p) for p in batch])}",
            "status": "pending",
            "command": full_command,
            "created_at": datetime.utcnow().isoformat() + "Z",
            "metadata": {
                "type": "sovereignty_batch",
                "batch_index": i,
                "targets": batch
            }
        }
        new_cards.append(card)
        print(f">> Generated Card: {batch_id} with {len(batch)} targets.")

    # 5. Append to Queue
    if os.path.exists(QUEUE_FILE):
        with open(QUEUE_FILE, "r") as f:
            try:
                queue = json.load(f)
            except json.JSONDecodeError:
                queue = []
    else:
        queue = []
    
    # Insert in reverse order to maintain sequence at the top of the queue
    for card in reversed(new_cards):
        queue.insert(0, card)
        
    with open(QUEUE_FILE, "w") as f:
        json.dump(queue, f, indent=2)
        
    print(f">> Successfully added {len(new_cards)} cards to {QUEUE_FILE}")

if __name__ == "__main__":
    generate_batches()
