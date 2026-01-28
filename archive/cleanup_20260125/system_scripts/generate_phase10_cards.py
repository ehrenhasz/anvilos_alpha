#!/usr/bin/env python3
import sqlite3
import json
import uuid
import os

# This script generates cards but does not submit them to the database.
# To submit, change SUBMIT_CARDS to True.
SUBMIT_CARDS = True
DB_PATH = "/var/lib/anvilos/db/cortex.db"
CARD_OUTPUT_DIR = "runtime/card_staging"

def generate_card_json(context, description, command):
    """Generates a JSON representation of a card."""
    correlation_id = f"anvil-{uuid.uuid4().hex[:8]}"
    card_payload = {
        "context": context,
        "details": command,
        "description": description,
        "source": "AIMEAT_PLANNER",
        "instruction": "SYSTEM_OP",
        "format": "shell"
    }
    job = {
        "correlation_id": correlation_id,
        "idempotency_key": f"op-{correlation_id}",
        "priority": 50,
        "cost_center": "OPS",
        "payload": json.dumps(card_payload),
        "status": "PENDING"
    }
    return job

def save_card(card_json):
    """Saves the card JSON to a file in the staging directory."""
    os.makedirs(CARD_OUTPUT_DIR, exist_ok=True)
    filepath = os.path.join(CARD_OUTPUT_DIR, f"{card_json['correlation_id']}.json")
    with open(filepath, 'w') as f:
        json.dump(card_json, f, indent=4)
    print(f"Card written to {filepath}")

def main():
    print("--- Generating cards for PHASE 10: ZFS Integration ---")
    
    cards = []
    
    # Task 1: compile_zfs_tools
    cards.append(generate_card_json(
        "compile_zfs_tools",
        "ZFS: Cross-compile ZFS userspace utilities",
        "system/scripts/build_zfs_tools.sh"
    ))

    # Task 2: integrate_zfs_kernel
    cards.append(generate_card_json(
        "integrate_zfs_kernel",
        "ZFS: Statically compile ZFS kernel module into the Anvil kernel",
        "system/scripts/integrate_zfs_kernel.sh"
    ))
    
    # Task 3: create_zfs_image_recipe
    cards.append(generate_card_json(
        "create_zfs_image_recipe",
        "ZFS: Create a ZFS root image from the ext4 host environment",
        "system/scripts/create_zfs_image.sh"
    ))
    
    # Task 4: update_iso_recipe
    cards.append(generate_card_json(
        "update_iso_recipe",
        "ZFS: Modify the ISO generation script to use the ZFS root image",
        "sed -i 's/OLD_ISO_LOGIC/NEW_ZFS_ISO_LOGIC/g' oss_sovereignty/sys_01_Linux_Kernel/source/build_iso.sh"
    ))

    if SUBMIT_CARDS:
        conn = sqlite3.connect(DB_PATH)
        for card in cards:
            conn.execute("""
                INSERT INTO jobs (correlation_id, idempotency_key, priority, cost_center, payload, status)
                VALUES (?, ?, ?, ?, ?, ?)
            """, (card['correlation_id'], card['idempotency_key'], card['priority'], card['cost_center'], card['payload'], card['status']))
        conn.commit()
        conn.close()
        print(f"{len(cards)} cards submitted to the database.")
    else:
        for card in cards:
            save_card(card)
        print(f"{len(cards)} card files generated. Submission is disabled.")

if __name__ == "__main__":
    main()
