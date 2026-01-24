
import os
import sys
import json
import sqlite3
import time
import uuid
import hashlib
from google import genai
from google.genai import types

# Fix path to import from runtime root if needed
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

# CONFIG
DB_PATH = os.path.join(os.path.dirname(__file__), "..", "cortex.db")
API_KEY = os.environ.get("GOOGLE_API_KEY") or open(os.path.join(os.path.dirname(__file__), "..", "..", "token")).read().strip()
MODEL_ID = "gemini-2.0-flash"

def get_db():
    return sqlite3.connect(DB_PATH)

def generate_id(content_str):
    return hashlib.md5(content_str.encode('utf-8')).hexdigest()

import subprocess
import shutil

# ... existing imports ...

def ensure_ocr(file_path):
    """
    Runs ocrmypdf on the file to ensure it has a text layer.
    Returns the path to the OCR'd file (or original if failed/skipped).
    """
    if not file_path.lower().endswith('.pdf'):
        return file_path
        
    print(f"[Butcher] Checking OCR status for: {file_path}")
    
    # Create temp path
    temp_ocr_path = file_path.replace('.pdf', '_ocr.pdf')
    
    # Check if we already have an OCR version (simple cache)
    if os.path.exists(temp_ocr_path):
        return temp_ocr_path

    try:
        # Run ocrmypdf with force-ocr to ensure we get a text layer even if one exists (it might be bad)
        # and deskew/clean for better vision parsing
        cmd = [
            "ocrmypdf",
            "--deskew",
            "--clean",
            "--force-ocr", 
            file_path,
            temp_ocr_path
        ]
        
        print(f"[Butcher] Running OCR: {' '.join(cmd)}")
        result = subprocess.run(
            cmd, 
            capture_output=True, 
            text=True
        )
        
        if result.returncode == 0:
            print(f"[Butcher] OCR Complete: {temp_ocr_path}")
            return temp_ocr_path
        else:
            # check if it failed because it's already done (exit code 6 usually means valid PDF but no OCR needed if skipped)
            if "already has text" in result.stderr:
                 print("[Butcher] File already has text layer. Skipping OCR.")
                 return file_path
                 
            print(f"[Butcher] OCR Failed: {result.stderr[:200]}")
            return file_path
            
    except Exception as e:
        print(f"[Butcher] OCR Tool Error: {e}")
        return file_path

def butcher_file(file_path):
    print(f"[Butcher] Processing: {file_path}")
    
    if not os.path.exists(file_path):
        print(f"[Butcher] ERROR: File not found {file_path}")
        return False

    # PRE-PROCESS: OCR
    processed_path = ensure_ocr(file_path)

    client = genai.Client(api_key=API_KEY)
    
    # 1. Upload File
    print("[Butcher] Uploading to Gemini...")
    try:
        # Check file size, if massive, might need chunking, but Flash handles ~1M tokens.
        # Simple upload for now.
        with open(processed_path, "rb") as f:
            file_content = f.read()
            
        upload_result = client.files.upload(
            file=processed_path,
            config=types.UploadFileConfig(display_name=os.path.basename(file_path))
        )
        print(f"[Butcher] Uploaded: {upload_result.name}")
        
        # 2. Prompt for Extraction
        prompt = """
        You are "The Butcher". Your job is to atomize this RPG sourcebook into discrete data fragments for a database.
        
        Extract the following asset types if found:
        - "Monster": Stat blocks (Name, HP, AC, Stats, Actions).
        - "Spell": Spell details (Name, Level, School, Casting Time, Description).
        - "Item": Magic items or equipment (Name, Type, Rarity, Mechanics).
        - "Rule": Discrete rule mechanics (Name, Description).
        - "Feat": Player options.
        - "ClassOption": Subclasses or Class features.

        Return a JSON object with a single key "fragments" containing a flat list of these objects.
        Each object MUST have:
        - "type": (One of the types above)
        - "name": (Name of the asset)
        - "data": (The full structured data content)
        - "raw_text": (The original text context for searching)

        Exclude generic fluff unless it is relevant to the mechanics.
        Output JSON ONLY.
        """

        print("[Butcher] Analyzing (this may take a moment)...")
        response = client.models.generate_content(
            model=MODEL_ID,
            contents=[
                types.Part.from_uri(file_uri=upload_result.uri, mime_type=upload_result.mime_type),
                prompt
            ],
            config=types.GenerateContentConfig(
                response_mime_type="application/json"
            )
        )
        
        # 3. Parse and Store
        try:
            result_json = json.loads(response.text)
            fragments = result_json.get("fragments", [])
            
            print(f"[Butcher] Extracted {len(fragments)} fragments.")
            
            with get_db() as conn:
                cursor = conn.cursor()
                
                for frag in fragments:
                    asset_type = frag.get("type", "Unknown")
                    name = frag.get("name", "Unknown")
                    raw_text = frag.get("raw_text", "")
                    data_json = json.dumps(frag.get("data", {}))
                    frag_id = str(uuid.uuid4())
                    
                    cursor.execute("""
                        INSERT INTO fragments (id, source_path, asset_type, name, content_json, raw_text)
                        VALUES (?, ?, ?, ?, ?, ?)
                    """, (frag_id, file_path, asset_type, name, data_json, raw_text))
                
                # Mark as butchered
                cursor.execute("UPDATE library_index SET butchered_at = CURRENT_TIMESTAMP WHERE path = ?", (file_path,))
                conn.commit()
                
            print("[Butcher] Success.")
            return True

        except json.JSONDecodeError:
            print(f"[Butcher] Failed to parse JSON response: {response.text[:200]}...")
            return False
            
    except Exception as e:
        print(f"[Butcher] CRITICAL FAILURE: {e}")
        return False

def run_loop(limit=1):
    count = 0
    with get_db() as conn:
        # fetchone needed because cursor iterator might be locked during updates if not careful, 
        # but simple select is fine.
        queue = conn.execute("SELECT path FROM view_butcher_queue LIMIT ?", (limit,)).fetchall()
        
    if not queue:
        print("[Butcher] Queue is empty.")
        return

    for row in queue:
        path = row[0]
        if butcher_file(path):
            count += 1
            
    print(f"[Butcher] Batch complete. Processed {count} files.")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        arg = sys.argv[1]
        if arg.isdigit():
            # queue mode with limit
            run_loop(limit=int(arg))
        else:
            # direct file mode
            butcher_file(arg)
    else:
        # default queue mode
        run_loop(limit=1)
