
import os
import shutil
import time
import json
import re
import sqlite3
import subprocess
from pathlib import Path

# --- PATHS ---
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(CURRENT_DIR, ".."))
USER_HOME = os.path.expanduser("~")
CORTEX_DIR = os.path.join(USER_HOME, ".dnd_dm")
CORTEX_DB_PATH = "/var/lib/anvilos/db/cortex.db"
TARGET_DIR = os.path.abspath(os.path.join(PROJECT_ROOT, "DND_Library"))
SUPPORTED_ARCHIVE_EXTENSIONS = ['.zip', '.rar', '.cbr', '.cbz']

def reindex_extracted_file(conn, file_path, metadata):
    """Adds a record for a single extracted file to the database."""
    filename = os.path.basename(file_path)
    try:
        conn.execute("""
            INSERT OR REPLACE INTO library_index (filename, path, tags, metadata)
            VALUES (?, ?, ?, ?)
        """, (filename, file_path, "Auto_Extracted", json.dumps(metadata)))
        print(f"[Backfill] Indexed extracted file: {filename}")
        return True
    except Exception as e:
        print(f"[Backfill] Error indexing extracted file {filename}: {e}")
        return False

def backfill_archives():
    """
    Scans the existing library_index for archives, extracts them, re-indexes their contents,
    and then removes the original archive entry and file.
    """
    if not os.path.exists(CORTEX_DB_PATH):
        print("[Backfill] Cortex DB not found. Nothing to do.")
        return

    print("[Backfill] Starting scan for existing archives in the library...")
    archives_to_process = []
    
    try:
        with sqlite3.connect(CORTEX_DB_PATH) as conn:
            conn.row_factory = sqlite3.Row
            cursor = conn.execute("SELECT id, filename, path, metadata FROM library_index")
            for row in cursor:
                _, ext = os.path.splitext(row['filename'])
                if ext.lower() in SUPPORTED_ARCHIVE_EXTENSIONS:
                    if os.path.exists(row['path']):
                        archives_to_process.append(dict(row))
                    else:
                        print(f"[Backfill] Found archive '{row['filename']}' in DB, but file is missing at '{row['path']}'. Removing entry.")
                        conn.execute("DELETE FROM library_index WHERE id = ?", (row['id'],))
            conn.commit() # Commit deletions if any
    except Exception as e:
        print(f"[Backfill] Error reading archives from DB: {e}")
        return

    # Filter out subsequent parts of multi-part RARs; prioritize .rar or .part01.rar
    unique_archives_to_process = {}
    for record in archives_to_process:
        filename = record['filename']
        # Regex to capture base name for multi-part rar (e.g., file.part01.rar, file.r00, file.rar)
        base_name_match = re.match(r'^(.*)\.part\d+\.rar$', filename, re.IGNORECASE) or \
                          re.match(r'^(.*)\.r\d+$|^(.+?)\.rar$', filename, re.IGNORECASE)

        if base_name_match:
            base_key = base_name_match.group(1) or base_name_match.group(2) # Get the common base name
            if base_key not in unique_archives_to_process:
                unique_archives_to_process[base_key] = record
            else:
                # If we find a 'part01' or '.rar' version, prefer it as the primary for extraction
                current_preferred_filename = unique_archives_to_process[base_key]['filename'].lower()
                if (
                    ('.part01.rar' in filename.lower() and '.part01.rar' not in current_preferred_filename) or \
                    (filename.lower().endswith('.rar') and not current_preferred_filename.endswith('.rar') and '.part' not in current_preferred_filename)
                   ):
                    unique_archives_to_process[base_key] = record
        else:
            # Non-multipart archives are added directly
            unique_archives_to_process[filename] = record

    archives_to_process = list(unique_archives_to_process.values())

    if not archives_to_process:
        print("[Backfill] No archives found in the database. Exiting.")
        return

    print(f"[Backfill] Found {len(archives_to_process)} archives to process (after filtering).")

    for archive_record in archives_to_process:
        archive_path = archive_record['path']
        archive_name = archive_record['filename']
        archive_dir = os.path.dirname(archive_path)
        archive_base_name, _ = os.path.splitext(archive_name)
        extract_dir = os.path.join(archive_dir, f"{archive_base_name}_extracted")
        
        print(f"[Backfill] Processing: {archive_name}")
        os.makedirs(extract_dir, exist_ok=True)

        try:
            ext = os.path.splitext(archive_name)[1].lower()
            if ext == '.zip' or ext == '.cbz':
                result = subprocess.run(['unzip', '-o', archive_path, '-d', extract_dir], check=True, capture_output=True, text=True)
            elif ext == '.rar' or ext == '.cbr':
                # For RAR, we typically only need to specify the first part
                # The unrar command will automatically find subsequent parts
                result = subprocess.run(['unrar', 'x', '-o+', archive_path, extract_dir], check=True, capture_output=True, text=True)
            else:
                print(f"[Backfill] Skipping unsupported archive type: {ext}")
                continue # Continue to next archive

            print(f"[Backfill] Extraction successful. Re-indexing contents...")
            
            with sqlite3.connect(CORTEX_DB_PATH) as conn:
                original_metadata = json.loads(archive_record['metadata'])
                
                for root, _, files in os.walk(extract_dir):
                    for file in files:
                        if file.startswith('.'): continue
                        full_path = os.path.join(root, file)
                        reindex_extracted_file(conn, full_path, original_metadata)
                
                conn.execute("DELETE FROM library_index WHERE id = ?", (archive_record['id'],))
                os.remove(archive_path)
                print(f"[Backfill] Successfully processed and removed original archive: {archive_name}")
        
        except subprocess.CalledProcessError as e:
            print(f"[Backfill] Extraction failed for {archive_name}: {e}")
            print(f"[Backfill] Stderr: {e.stderr}")
            print(f"[Backfill] Stdout: {e.stdout}")
            shutil.rmtree(extract_dir, ignore_errors=True)
        except FileNotFoundError:
            print(f"[Backfill] Extraction tool not found for {archive_name}. Please ensure 'unzip' and 'unrar' are installed.")
            shutil.rmtree(extract_dir, ignore_errors=True)
        except Exception as e:
            print(f"[Backfill] An unexpected error occurred for {archive_name}: {e}. Cleaning up extracted directory.")
            shutil.rmtree(extract_dir, ignore_errors=True)
        conn.commit()
