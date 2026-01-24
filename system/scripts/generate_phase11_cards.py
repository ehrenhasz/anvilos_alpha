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
    print("--- Generating cards for PHASE 11: Shell Standardization ---")
    
    cards = []
    
    # Task 1: assimilate_bash
    cards.append(generate_card_json(
        "assimilate_bash",
        "SHELL: Assimilate GNU Bash source into oss_sovereignty",
        "git clone https://git.savannah.gnu.org/git/bash.git oss_sovereignty/sys_13_Shells/bash/source"
    ))
    
    # Task 2: assimilate_micropython
    cards.append(generate_card_json(
        "assimilate_micropython",
        "SHELL: Verify and create manifest for MicroPython userland source",
        "ls -l oss_sovereignty/sys_09_Anvil/source/py && echo 'MicroPython source present' > oss_sovereignty/sys_13_Shells/micropython/manifest.txt"
    ))
    
    # Task 3: configure_kernel_shells
    cards.append(generate_card_json(
        "configure_kernel_shells",
        "SHELL: Modify kernel configuration for restricted shell support",
        "system/scripts/configure_shells.sh"
    ))
    
    # Task 4: purge_unauthorized_shells
    cards.append(generate_card_json(
        "purge_unauthorized_shells",
        "SHELL: Purge unauthorized shell binaries from the final rootfs",
        "system/scripts/purge_shells.sh"
    ))
    
    # Task 5: validate_final_shells
    cards.append(generate_card_json(
        "validate_final_shells",
        "SHELL: Add final validation step to ensure only bash and python are present",
        "system/scripts/validate_shells.sh"
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
