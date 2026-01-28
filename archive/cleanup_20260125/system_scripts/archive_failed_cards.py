import json
import os
from datetime import datetime

QUEUE_FILE = "runtime/card_queue.json"
ARCHIVE_FILE = "runtime/card_archive_failed.json"

def archive_failed():
    if not os.path.exists(QUEUE_FILE):
        print("Queue file not found.")
        return

    try:
        with open(QUEUE_FILE, "r") as f:
            queue = json.load(f)
    except json.JSONDecodeError:
        print("Error reading queue file.")
        return

    failed_cards = [c for c in queue if c.get("status") == "failed"]
    active_cards = [c for c in queue if c.get("status") != "failed"]

    if not failed_cards:
        print("No failed cards to archive.")
        return

    # Load existing archive or create new
    if os.path.exists(ARCHIVE_FILE):
        with open(ARCHIVE_FILE, "r") as f:
            try:
                archive = json.load(f)
            except json.JSONDecodeError:
                archive = []
    else:
        archive = []

    # Add timestamp to archived cards
    archive_time = datetime.now().isoformat()
    for card in failed_cards:
        card["archived_at"] = archive_time
        archive.append(card)

    # Write back
    with open(QUEUE_FILE, "w") as f:
        json.dump(active_cards, f, indent=2)
    
    with open(ARCHIVE_FILE, "w") as f:
        json.dump(archive, f, indent=2)

    print(f">> Archived {len(failed_cards)} failed cards to {ARCHIVE_FILE}")

if __name__ == "__main__":
    archive_failed()
