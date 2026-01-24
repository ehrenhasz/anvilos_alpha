
import os
import sqlite3
import json
import re
from pathlib import Path
from google import genai
from google.genai import types

DB_PATH = os.path.join(os.path.dirname(os.path.dirname(__file__)), "cortex.db")
LIBRARY_ROOT = os.path.abspath("DND_Library")
TOKEN_PATH = os.path.join(os.path.dirname(os.path.dirname(__file__)), "config", "token")

def get_api_key():
    if os.path.exists(TOKEN_PATH):
        with open(TOKEN_PATH, "r") as f:
            return f.read().strip()
    return os.environ.get("GOOGLE_API_KEY")

API_KEY = get_api_key()

def init_db():
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute("""
            CREATE TABLE IF NOT EXISTS library_index (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                filename TEXT,
                path TEXT UNIQUE,
                file_size INTEGER,
                metadata JSON,
                tags TEXT,
                status TEXT DEFAULT 'INDEXED'
            )
        """)
        conn.commit()

def classify_file_ai(filename):
    if not API_KEY:
        return {}
    
    prompt = f"""
    Analyze filename: "{filename}"
    Return JSON with: {{
        "title": "Clean Title",
        "system": "D&D 5e/PF2e/OSR/etc",
        "type": "Rulebook/Adventure/Map/etc",
        "author": "Publisher/Author"
    }}
    """
    try:
        client = genai.Client(api_key=API_KEY)
        response = client.models.generate_content(
            model="gemini-2.0-flash",
            contents=prompt,
            config=types.GenerateContentConfig(response_mime_type="application/json")
        )
        return json.loads(response.text)
    except:
        return {"title": filename}

def scan_library():
    init_db()
    print(f"[Library] Scanning {LIBRARY_ROOT}...")
    
    count = 0
    with sqlite3.connect(DB_PATH) as conn:
        for root, dirs, files in os.walk(LIBRARY_ROOT):
            for file in files:
                if file.startswith("."): continue
                
                full_path = os.path.join(root, file)
                rel_path = os.path.relpath(full_path, LIBRARY_ROOT)
                
                # Check if exists
                exists = conn.execute("SELECT 1 FROM library_index WHERE path=?", (full_path,)).fetchone()
                if exists:
                    continue
                
                # Basic Meta from path
                # Expecting: DND_Library/License/System/Publisher/Type/File
                parts = rel_path.split(os.sep)
                meta = {
                    "license": parts[0] if len(parts) > 0 else "Unknown",
                    "system": parts[1] if len(parts) > 1 else "Unknown",
                    "publisher": parts[2] if len(parts) > 2 else "Unknown",
                    "type": parts[3] if len(parts) > 3 else "Unknown"
                }
                
                # AI Enhancement for Adventures/Rulebooks
                if meta["type"] in ["Adventures", "Rulebooks", "Sourcebooks"]:
                    ai_meta = classify_file_ai(file)
                    meta.update(ai_meta)
                
                conn.execute("""
                    INSERT INTO library_index (filename, path, file_size, metadata, tags)
                    VALUES (?, ?, ?, ?, ?)
                """, (file, full_path, os.path.getsize(full_path), json.dumps(meta), meta["system"]))
                
                count += 1
                if count % 10 == 0:
                    print(f"[Library] Indexed {count} files...")
                    conn.commit()
        conn.commit()
    print(f"[Library] Scan Complete. Added {count} new files.")

if __name__ == "__main__":
    scan_library()
