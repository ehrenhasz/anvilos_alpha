import os
import json
import sqlite3
from pathlib import Path
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(CURRENT_DIR, ".."))
USER_HOME = os.path.expanduser("~")
CORTEX_DIR = os.path.join(USER_HOME, ".dnd_dm")
CORTEX_DB_PATH = "/var/lib/anvilos/db/cortex.db"
TARGET_DIR = os.path.abspath(os.path.join(PROJECT_ROOT, "DND_Library"))
def extract_metadata_from_path(file_path):
    """
    Extracts metadata (license, system, publisher, product_type) from the file's path
    within the DND_Library structure.
    """
    relative_path = Path(file_path).relative_to(TARGET_DIR)
    parts = list(relative_path.parts)
    meta = {
        "license": "Unsorted",
        "system_or_genre": "Unknown",
        "publisher": "Unknown",
        "product_type": "Misc"
    }
    if len(parts) >= 1:
        meta["license"] = parts[0]
    if len(parts) >= 2:
        meta["system_or_genre"] = parts[1]
    if len(parts) >= 3:
        meta["publisher"] = parts[2]
    if len(parts) >= 4:
        meta["product_type"] = parts[3]
    return meta
def rebuild_index():
    """
    Scans the DND_Library, extracts metadata from paths, and adds missing files
    to the library_index table without moving or deleting files.
    """
    print(f"[Reindex] Starting library index rebuild for: {TARGET_DIR}")
    if not os.path.exists(CORTEX_DB_PATH):
        os.makedirs(os.path.dirname(CORTEX_DB_PATH), exist_ok=True)
        conn = sqlite3.connect(CORTEX_DB_PATH)
        conn.close() # Create the DB file if it doesn't exist
    try:
        with sqlite3.connect(CORTEX_DB_PATH) as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS library_index (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    filename TEXT UNIQUE,
                    path TEXT,
                    tags TEXT,
                    metadata TEXT,
                    added_at DATETIME DEFAULT CURRENT_TIMESTAMP
                )
            """)
            conn.commit()
            indexed_files = set()
            cursor = conn.execute("SELECT filename FROM library_index")
            for row in cursor:
                indexed_files.add(row[0])
            for root, dirs, files in os.walk(TARGET_DIR):
                for file in files:
                    if file.startswith("."): # Ignore hidden files
                        continue
                    full_path = os.path.join(root, file)
                    if file not in indexed_files:
                        print(f"[Reindex] Found new file: {file}")
                        meta = extract_metadata_from_path(full_path)
                        try:
                            conn.execute("""
                                INSERT OR REPLACE INTO library_index (filename, path, tags, metadata)
                                VALUES (?, ?, ?, ?)
                            """, (file, full_path, "Manual_Reindex", json.dumps(meta)))
                            conn.commit()
                            print(f"[Reindex] Added {file} to index.")
                        except sqlite3.IntegrityError:
                            print(f"[Reindex] Warning: {file} already exists in index, skipping.")
                        except Exception as db_err:
                            print(f"[Reindex] Error adding {file} to DB: {db_err}")
    except Exception as e:
        print(f"[Reindex] Critical Error: {e}")
if __name__ == "__main__":
    rebuild_index()
