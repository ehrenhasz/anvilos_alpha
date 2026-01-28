import os
import shutil
import time
import json
import re
import sqlite3
import subprocess
from pathlib import Path
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(CURRENT_DIR, ".."))
USER_HOME = os.path.expanduser("~")
CORTEX_DIR = os.path.join(USER_HOME, ".dnd_dm")
CORTEX_DB_PATH = "/var/lib/anvilos/db/cortex.db"
SOURCE_DIR = os.path.abspath(os.path.join(PROJECT_ROOT, "dnd_import"))
TARGET_DIR = os.path.abspath(os.path.join(PROJECT_ROOT, "DND_Library"))
SUPPORTED_ARCHIVE_EXTENSIONS = ['.zip', '.rar', '.cbr', '.cbz']
def get_api_key():
    token_path = os.path.join(PROJECT_ROOT, "config", "token")
    if os.path.exists(token_path):
        with open(token_path, "r") as f:
            return f.read().strip()
    return os.environ.get("GOOGLE_API_KEY")
API_KEY = get_api_key()
def classify_file(filename):
    if not API_KEY:
        print("[Force Import] Warning: No API Key found. Using default classification.")
        return {"license": "Unsorted", "system": "Unknown", "type": "Misc", "publisher": "Unknown"}
    try:
        from google import genai
        client = genai.Client(api_key=API_KEY)
        response = client.models.generate_content(
            model="gemini-2.0-flash",
            contents=f"Classify the file '{filename}' into license, system, publisher, and product_type.",
            config=genai.types.GenerateContentConfig(response_mime_type="application/json")
        )
        return json.loads(response.text)
    except Exception as e:
        print(f"[Force Import] AI Classification failed for {filename}: {e}")
        return {"license": "Unsorted", "system": "Unknown", "type": "Misc", "publisher": "Unknown"}
def process_single_file(file_path, dest_dir_meta):
    """Processes a single, non-archive file."""
    try:
        file = os.path.basename(file_path)
        target_file = os.path.join(dest_dir_meta, file)
        shutil.copy2(file_path, target_file)
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
            meta = classify_file(file)
            conn.execute("""
                INSERT OR REPLACE INTO library_index (filename, path, tags, metadata)
                VALUES (?, ?, ?, ?)
            """, (file, target_file, "Forced_Import", json.dumps(meta)))
        print(f"[Force Import] Indexed single file: {file}")
        os.remove(file_path) # Remove original after successful processing
        return True
    except Exception as e:
        print(f"[Force Import] Error indexing single file {file}: {e}")
        if 'target_file' in locals() and os.path.exists(target_file):
            os.remove(target_file) # Clean up copied file on failure
        return False
def process_archive(archive_path, dest_dir_base):
    """Extracts and processes an archive file."""
    archive_name = os.path.basename(archive_path)
    archive_base_name, _ = os.path.splitext(archive_name)
    extract_dir = os.path.join(dest_dir_base, f"{archive_base_name}_extracted")
    os.makedirs(extract_dir, exist_ok=True)
    print(f"[Force Import] Extracting '{archive_name}' to '{extract_dir}'...")
    try:
        ext = os.path.splitext(archive_name)[1].lower()
        if ext == '.zip' or ext == '.cbz':
            subprocess.run(['unzip', '-o', archive_path, '-d', extract_dir], check=True, capture_output=True)
        elif ext == '.rar' or ext == '.cbr':
            subprocess.run(['unrar', 'x', '-o+', archive_path, extract_dir], check=True, capture_output=True)
        else:
            print(f"[Force Import] Skipping unsupported archive type: {ext}")
            return False
        print(f"[Force Import] Extraction successful. Indexing contents...")
        for root, _, files in os.walk(extract_dir):
            for file in files:
                if file.startswith('.'): continue
                full_path = os.path.join(root, file)
                if not process_single_file(full_path, extract_dir):
                     print(f"[Force Import] Failed to process extracted file: {file}")
        os.remove(archive_path) # Remove original archive after successful processing
        return True
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        print(f"[Force Import] Extraction failed for {archive_name}: {e}")
        shutil.rmtree(extract_dir)
        return False
    except Exception as e:
        print(f"[Force Import] An unexpected error occurred during archive processing: {e}")
        shutil.rmtree(extract_dir)
        return False
def force_import():
    """Recursively scans the dnd_import directory and processes all files."""
    print(f"[Force Import] Starting scan of {SOURCE_DIR}")
    if not os.path.exists(SOURCE_DIR):
        print(f"[Force Import] Source directory not found: {SOURCE_DIR}")
        return
    for root, dirs, files in os.walk(SOURCE_DIR):
        for file in files:
            if file.startswith("."):
                continue
            source_path = os.path.join(root, file)
            try:
                clean_file = " ".join(file.split()).strip()
                if not clean_file:
                    print(f"[Force Import] Skipped file with invalid characters: {repr(file)}")
                    continue
                if clean_file != file:
                    print(f"[Force Import] Sanitizing filename from '{repr(file)}' to '{clean_file}'")
                    new_source_path = os.path.join(root, clean_file)
                    os.rename(source_path, new_source_path)
                    source_path = new_source_path
                    file = clean_file
                print(f"[Force Import] Processing: {file}")
                meta = classify_file(file)
                dest_path_base = os.path.join(
                    TARGET_DIR,
                    meta.get("license", "Unsorted"),
                    meta.get("system_or_genre", "Unknown"),
                    meta.get("publisher", "Unknown"),
                    meta.get("product_type", "Misc")
                )
                os.makedirs(dest_path_base, exist_ok=True)
                _, ext = os.path.splitext(file)
                if ext.lower() in SUPPORTED_ARCHIVE_EXTENSIONS:
                    process_archive(source_path, dest_path_base)
                else:
                    process_single_file(source_path, dest_path_base)
            except Exception as e:
                print(f"[Force Import] CRITICAL ERROR processing file '{file}': {e}")
                continue
    print("[Force Import] Scan complete.")
if __name__ == "__main__":
    force_import()
