#!/usr/bin/env python3
import sqlite3
import json
import os
import time

DB_PATH = "/var/lib/anvilos/db/cortex.db"
ARCHIVE_PATH = "runtime/card_archive_jammed.json"

def archive_jammed_cards():
    if not os.path.exists(DB_PATH):
        print(f"Database not found at {DB_PATH}")
        return

    try:
        with sqlite3.connect(DB_PATH) as conn:
            conn.row_factory = sqlite3.Row
            
            # Select jammed cards
            cursor = conn.execute("SELECT * FROM card_stack WHERE stat=9")
            rows = cursor.fetchall()
            
            if not rows:
                print("No jammed cards found to archive.")
                return

            print(f"Found {len(rows)} jammed cards.")
            
            jammed_cards = [dict(row) for row in rows]
            
            # Load existing archive if it exists
            existing_archive = []
            if os.path.exists(ARCHIVE_PATH):
                try:
                    with open(ARCHIVE_PATH, 'r') as f:
                        existing_archive = json.load(f)
                except json.JSONDecodeError:
                    print(f"Warning: {ARCHIVE_PATH} is corrupted. Starting fresh.")

            # Append new cards
            existing_archive.extend(jammed_cards)
            
            # Write back to archive file
            with open(ARCHIVE_PATH, 'w') as f:
                json.dump(existing_archive, f, indent=2)
            
            print(f"Archived {len(jammed_cards)} cards to {ARCHIVE_PATH}")

            # Delete from DB
            conn.execute("DELETE FROM card_stack WHERE stat=9")
            print("Deleted jammed cards from database.")

    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    archive_jammed_cards()
