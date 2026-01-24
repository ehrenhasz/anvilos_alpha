#!/usr/bin/env python3
import os
import sys
import sqlite3
import json
import shutil
import time
import re
import logging
from google import genai
from google.genai import types

# --- CONFIG ---
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIBRARY_DIR = os.path.join(BASE_DIR, "DND_Library")
DB_PATH = os.path.join(BASE_DIR, "runtime", "cortex.db")
TOKEN_PATH = os.path.join(BASE_DIR, "config", "token")

# Setup Logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - [ARCHIVIST] - %(levelname)s - %(message)s',
    handlers=[logging.StreamHandler(sys.stdout)]
)
logger = logging.getLogger()

def get_api_key():
    if os.path.exists(TOKEN_PATH):
        with open(TOKEN_PATH, 'r') as f:
            return f.read().strip()
    return os.environ.get("GOOGLE_API_KEY")

API_KEY = get_api_key()

def init_db():
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute("""
            CREATE TABLE IF NOT EXISTS library_index (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                filename TEXT UNIQUE,
                path TEXT,
                metadata TEXT,
                status TEXT DEFAULT 'PENDING', -- PENDING, INDEXED, ENRICHED, MOVED
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        """)

def scan_library():
    """Walks the library and registers files in the DB."""
    logger.info("Scanning library for new files...")
    count = 0
    with sqlite3.connect(DB_PATH) as conn:
        for root, _, files in os.walk(LIBRARY_DIR):
            for file in files:
                if file.startswith('.'): continue
                full_path = os.path.join(root, file)
                
                # Check if exists
                exists = conn.execute("SELECT id FROM library_index WHERE filename = ?", (file,)).fetchone()
                if not exists:
                    conn.execute("INSERT INTO library_index (filename, path, status) VALUES (?, ?, 'PENDING')", (file, full_path))
                    count += 1
    
    logger.info(f"Scan complete. Registered {count} new files.")

def get_metadata_from_gemini(filename):
    if not API_KEY:
        logger.warning("No API Key available for metadata enrichment.")
        return None

    client = genai.Client(api_key=API_KEY)
    
    prompt = f"""
    You are a Librarian for a Tabletop RPG archive.
    Analyze this filename: "{filename}"
    
    Your goal is to identify the product and provide metadata fitting the structure of a standardized RPG library (like DriveThruRPG's categories).
    
    Return a JSON object with these keys:
    {{
        "title": "The official product title",
        "publisher": "Publisher Name (e.g. Wizards of the Coast, Paizo, Kobold Press)",
        "system": "The Game System (e.g. Dungeons & Dragons 5e, Pathfinder 2e, OSR, System Neutral)",
        "edition": "Edition if applicable (e.g. 5e, 3.5, 2e)",
        "type": "Product Type (Core Rulebook, Adventure, Supplement, Map, Card Deck)",
        "license": "License Category (Official, OGL, Community, Homebrew)",
        "author": "Primary Author or 'Various'"
    }}
    
    Rules:
    - If you are confident it is an official D&D product, label it 'Official'.
    - If it is from a 3rd party publisher for D&D, label it 'OGL'.
    - If you cannot identify it, do your best guess based on the name. 
    
    Return ONLY valid JSON.
    """
    
    try:
        response = client.models.generate_content(
            model="gemini-2.0-flash",
            contents=prompt,
            config=types.GenerateContentConfig(response_mime_type="application/json")
        )
        
        text = response.text.strip()
        # Clean markdown
        if text.startswith("```"):
            text = re.sub(r'^```json\s*|\s*```$', '', text, flags=re.MULTILINE)
            
        return json.loads(text)
    except Exception as e:
        logger.error(f"Gemini Error for {filename}: {e}")
        return None

def enrich_files():
    """Finds files with missing metadata and asks Gemini."""
    logger.info("Starting Enrichment Cycle...")
    
    with sqlite3.connect(DB_PATH) as conn:
        conn.row_factory = sqlite3.Row
        # Removed LIMIT 50 to allow full processing, but we'll still process in chunks internally if needed
        # Actually, let's keep a reasonable batch size per DB transaction for safety
        while True:
            items = conn.execute("""
                SELECT id, filename, path FROM library_index 
                WHERE status = 'PENDING' 
                   OR (path LIKE '%/Unsorted/%' AND status != 'ENRICHED')
                LIMIT 100
            """).fetchall()
            
            if not items:
                logger.info("No more files need enrichment in this pass.")
                break

            for item in items:
                filename = item['filename']
                logger.info(f"Enriching: {filename}")
                
                meta = get_metadata_from_gemini(filename)
                if meta:
                    if isinstance(meta, list) and len(meta) > 0:
                        meta = meta[0]
                    
                    if not isinstance(meta, dict):
                        logger.error(f"Invalid metadata format for {filename}")
                        conn.execute("UPDATE library_index SET status = 'FAILED' WHERE id = ?", (item['id'],))
                        conn.commit()
                        continue

                    # Sanitize keys for filesystem
                    for k, v in meta.items():
                        if isinstance(v, str):
                            meta[k] = re.sub(r'[<>:"/\\|?*]', '', v).strip()

                    conn.execute(
                        "UPDATE library_index SET metadata = ?, status = 'ENRICHED' WHERE id = ?",
                        (json.dumps(meta), item['id'])
                    )
                    conn.commit()
                    logger.info(f"Updated metadata for {filename}")
                    time.sleep(0.5) # Slight delay for rate limit
                else:
                    conn.execute("UPDATE library_index SET status = 'FAILED' WHERE id = ?", (item['id'],))
                    conn.commit()

def reorganize_files():
    """Moves files based on metadata."""
    logger.info("Starting Reorganization Cycle...")
    
    with sqlite3.connect(DB_PATH) as conn:
        conn.row_factory = sqlite3.Row
        # Process all enriched items
        while True:
            items = conn.execute("SELECT id, filename, path, metadata FROM library_index WHERE status = 'ENRICHED' LIMIT 500").fetchall()
            if not items: break
            
            for item in items:
                try:
                    meta = json.loads(item['metadata'])
                    current_path = item['path']
                    filename = item['filename']
                    
                    license_dir = meta.get('license', 'Unsorted')
                    system_dir = meta.get('system', 'Unknown')
                    publisher_dir = meta.get('publisher', 'Unknown')
                    type_dir = meta.get('type', 'Misc')
                    
                    target_dir = os.path.join(LIBRARY_DIR, license_dir, system_dir, publisher_dir, type_dir)
                    target_path = os.path.join(target_dir, filename)
                    
                    if os.path.abspath(current_path) != os.path.abspath(target_path):
                        if os.path.exists(current_path):
                            os.makedirs(target_dir, exist_ok=True)
                            shutil.move(current_path, target_path)
                            
                            conn.execute("UPDATE library_index SET path = ?, status = 'MOVED' WHERE id = ?", (target_path, item['id']))
                            logger.info(f"Moved: {filename}")
                        else:
                            conn.execute("UPDATE library_index SET status = 'LOST' WHERE id = ?", (item['id'],))
                    else:
                        conn.execute("UPDATE library_index SET status = 'MOVED' WHERE id = ?", (item['id'],))
                except Exception as e:
                    logger.error(f"Error Reorganizing {item['filename']}: {e}")
            conn.commit()

def cleanup_empty_dirs():
    """Removes empty directories in DND_Library."""
    logger.info("Cleaning up empty directories...")
    # Walk bottom-up to catch nested empty dirs
    for root, dirs, files in os.walk(LIBRARY_DIR, topdown=False):
        for name in dirs:
            path = os.path.join(root, name)
            if not os.listdir(path): 
                try:
                    os.rmdir(path)
                except OSError:
                    pass

def main():
    init_db()
    logger.info("Archivist Run Started.")
    
    scan_library()
    enrich_files()
    reorganize_files()
    cleanup_empty_dirs()
    
    logger.info("Archivist Run Complete.")

if __name__ == "__main__":
    main()
