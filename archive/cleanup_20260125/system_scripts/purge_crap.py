import json
import os

CARD_PATH = "runtime/card_queue.json"

def purge_legacy_crap():
    if not os.path.exists(CARD_PATH):
        return

    with open(CARD_PATH, "r") as f:
        queue = json.load(f)

    # Keywords to identify "legacy crap" we don't need
    crap_keywords = ["sys_18_64Bit_Cloud", "sys_17_32Bit_Era", "sys_fix_failed_sov_paths"]
    
    initial_count = len(queue)
    # Filter out any card that mentions these paths in the command or ID
    clean_queue = []
    for card in queue:
        card_str = json.dumps(card)
        if any(kw in card_str for kw in crap_keywords):
            print(f">> Purging Card: {card.get('id')} ({card.get('description')})")
            continue
        clean_queue.append(card)

    removed_count = initial_count - len(clean_queue)
    
    with open(CARD_PATH, "w") as f:
        json.dump(clean_queue, f, indent=2)
    
    print(f">> Purged {removed_count} legacy cards. Queue is now clean.")

if __name__ == "__main__":
    purge_legacy_crap()
