#!/usr/bin/env python3
import os
import sys
import sqlite3
import json
import uuid
import logging
import mimetypes
from datetime import datetime

# --- CONFIGURATION ---
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIBRARY_DIR = os.path.join(BASE_DIR, "DND_Library")
DB_PATH = os.path.join(BASE_DIR, "runtime", "dnd_dm.db")

# Logging setup
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - [THE BUTCHER] - %(levelname)s - %(message)s',
    handlers=[logging.StreamHandler(sys.stdout)]
)
logger = logging.getLogger()

def init_db():
    """Initializes the VTT-ready Asset Database."""
    with sqlite3.connect(DB_PATH) as conn:
        # Enable FK support
        conn.execute("PRAGMA foreign_keys = ON")
        
        # 1. Asset Types (Lookup)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS asset_types (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT UNIQUE NOT NULL
            )
        """)
        # Seed basic types
        types = ['Sourcebook', 'Map', 'Token', 'Portrait', 'Audio', 'Monster', 'Spell', 'Item', 'Rule', 'RollableTable']
        for t in types:
            conn.execute("INSERT OR IGNORE INTO asset_types (name) VALUES (?)", (t,))
            
        # 2. Assets (The Core Table)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS assets (
                id TEXT PRIMARY KEY, -- UUID
                parent_id TEXT, -- For atomized assets (e.g. Monster from Sourcebook)
                type_id INTEGER,
                name TEXT NOT NULL,
                file_path TEXT, -- Relative to DND_Library or null if purely data
                metadata TEXT, -- JSONB equivalent (System, Publisher, License, Stats)
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY(type_id) REFERENCES asset_types(id),
                FOREIGN KEY(parent_id) REFERENCES assets(id)
            )
        """)
        
        # 3. Tags
        conn.execute("""
            CREATE TABLE IF NOT EXISTS tags (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT UNIQUE NOT NULL
            )
        """)
        
        # 4. Asset Tags (Many-to-Many)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS asset_tags (
                asset_id TEXT,
                tag_id INTEGER,
                PRIMARY KEY (asset_id, tag_id),
                FOREIGN KEY(asset_id) REFERENCES assets(id),
                FOREIGN KEY(tag_id) REFERENCES tags(id)
            )
        """)
        
        logger.info("Database schema initialized.")

def get_or_create_tag(conn, tag_name):
    """Returns tag_id, creating if necessary."""
    tag_name = tag_name.strip()
    if not tag_name: return None
    
    cur = conn.execute("SELECT id FROM tags WHERE name = ?", (tag_name,))
    row = cur.fetchone()
    if row:
        return row[0]
    
    try:
        cur = conn.execute("INSERT INTO tags (name) VALUES (?)", (tag_name,))
        return cur.lastrowid
    except sqlite3.IntegrityError:
        # Race condition handling
        return conn.execute("SELECT id FROM tags WHERE name = ?", (tag_name,)).fetchone()[0]

def determine_type(filename, path_parts):
    """Infers asset type from extension and path context."""
    ext = os.path.splitext(filename)[1].lower()
    path_str = "/".join(path_parts).lower()
    
    if ext in ['.pdf', '.epub', '.txt', '.md']:
        return 'Sourcebook'
    
    if ext in ['.jpg', '.jpeg', '.png', '.webp', '.gif']:
        if 'map' in path_str: return 'Map'
        if 'token' in path_str: return 'Token'
        if 'portrait' in path_str: return 'Portrait'
        return 'Visual' # Generic
        
    if ext in ['.mp3', '.wav', '.ogg', '.flac']:
        return 'Audio'
        
    return 'Unknown'

def butcher_library():
    """
    Reads ENRICHED assets from Cortex DB (Archivist output) and ingests them into the VTT DB.
    """
    cortex_db_path = os.path.join(BASE_DIR, "runtime", "cortex.db")
    
    if not os.path.exists(cortex_db_path):
        logger.error("Cortex DB not found. Run the Archivist first.")
        return

    logger.info(f"Butchering from Metadata Source: {cortex_db_path}...")
    
    with sqlite3.connect(DB_PATH) as vtt_conn, sqlite3.connect(cortex_db_path) as meta_conn:
        vtt_conn.execute("PRAGMA foreign_keys = ON")
        meta_conn.row_factory = sqlite3.Row
        
        # Load VTT Asset Types
        type_map = {row[1]: row[0] for row in vtt_conn.execute("SELECT id, name FROM asset_types")}
        
        # Fetch Enriched/Moved items from Archivist
        rows = meta_conn.execute("SELECT filename, path, metadata FROM library_index WHERE status IN ('ENRICHED', 'MOVED')").fetchall()
        
        count = 0
        for row in rows:
            filename = row['filename']
            full_path = row['path']
            raw_meta = row['metadata']
            
            # Skip if file doesn't exist on disk (drift)
            if not os.path.exists(full_path):
                continue
                
            rel_path = os.path.relpath(full_path, LIBRARY_DIR)
            
            # Check if already ingested in VTT DB
            existing = vtt_conn.execute("SELECT id FROM assets WHERE file_path = ?", (rel_path,)).fetchone()
            if existing:
                continue

            try:
                meta = json.loads(raw_meta)
            except json.JSONDecodeError:
                continue

            # Map Metadata to Tags and Type
            tags = []
            if 'license' in meta: tags.append(meta['license'])
            if 'system' in meta: tags.append(meta['system'])
            if 'publisher' in meta: tags.append(meta['publisher'])
            
            # Determine Asset Type
            # Archivist gives 'type', e.g., 'Core Rulebook', 'Adventure'
            meta_type = meta.get('type', 'Unknown')
            
            # Map Archivist Type to VTT Asset Type
            vtt_type_name = 'Sourcebook' # Default for PDFs/Text
            ext = os.path.splitext(filename)[1].lower()
            
            if 'Map' in meta_type or 'map' in meta_type.lower(): vtt_type_name = 'Map'
            elif 'Token' in meta_type: vtt_type_name = 'Token'
            elif ext in ['.jpg', '.png', '.webp']: vtt_type_name = 'Visual'
            elif ext in ['.mp3', '.ogg']: vtt_type_name = 'Audio'
            
            type_id = type_map.get(vtt_type_name)
            if not type_id:
                vtt_conn.execute("INSERT OR IGNORE INTO asset_types (name) VALUES (?)", (vtt_type_name,))
                type_id = vtt_conn.execute("SELECT id FROM asset_types WHERE name = ?", (vtt_type_name,)).fetchone()[0]
                type_map[vtt_type_name] = type_id

            asset_id = str(uuid.uuid4())
            
            vtt_conn.execute("""
                INSERT INTO assets (id, type_id, name, file_path, metadata)
                VALUES (?, ?, ?, ?, ?)
            """, (asset_id, type_id, filename, rel_path, json.dumps(meta)))
            
            # Apply Tags
            for t in tags:
                tag_id = get_or_create_tag(vtt_conn, t)
                if tag_id:
                    vtt_conn.execute("INSERT OR IGNORE INTO asset_tags (asset_id, tag_id) VALUES (?, ?)", (asset_id, tag_id))
            
            count += 1
            if count % 100 == 0:
                vtt_conn.commit()
                logger.info(f"Ingested {count} assets from metadata...")

        vtt_conn.commit()
        logger.info(f"Butcher Run Complete. Ingested {count} new assets based on Metadata.")

if __name__ == "__main__":
    init_db()
    butcher_library()
