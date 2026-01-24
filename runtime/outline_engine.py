
import os
import json
import sqlite3
import uuid
import random
from typing import List, Dict, Optional

APP_DB = "/var/lib/anvilos/db/dnd_dm.db"

class OutlineEngine:
    def __init__(self, db_path: str):
        self.db_path = db_path
        self._init_db()

    def _init_db(self):
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS story_outlines (
                    id TEXT PRIMARY KEY,
                    campaign_mode TEXT,
                    source_file TEXT,
                    title TEXT,
                    content_json TEXT,
                    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
                )
            """)
            conn.execute("""
                CREATE TABLE IF NOT EXISTS session_state (
                    session_id TEXT PRIMARY KEY,
                    outline_id TEXT,
                    current_node_id TEXT,
                    turn_count INTEGER DEFAULT 0,
                    state_json TEXT,
                    history_json TEXT,
                    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
                )
            """)

    def _fetch_random_fragments(self, limit: int = 5) -> List[Dict]:
        """Fetch random fragments from the Butcher's DB for Remix Mode."""
        if not os.path.exists(APP_DB):
            return []
        with sqlite3.connect(APP_DB) as conn:
            conn.row_factory = sqlite3.Row
            rows = conn.execute(
                "SELECT name, asset_type, raw_text FROM fragments ORDER BY RANDOM() LIMIT ?", 
                (limit,)
            ).fetchall()
            return [dict(r) for r in rows]

    def generate_outline(self, mode: str, prompt: str = "", source_file: str = "") -> str:
        """
        Generates a story outline based on the campaign mode.
        """
        outline_id = str(uuid.uuid4())
        title = "New Adventure"
        nodes = []

        if mode == "A":
            # Strict Mode: Parse from source_file (Expecting JSON structure)
            title = f"Adventure: {os.path.basename(source_file)}"
            if os.path.exists(source_file):
                try:
                    with open(source_file, 'r') as f:
                        data = json.load(f)
                        nodes = data.get('nodes', [])
                        title = data.get('title', title)
                except Exception as e:
                    print(f"Error loading source file: {e}")
                    nodes = [{"id": "err", "title": "Load Error", "content": "Could not load strict module."}]
            else:
                 nodes = [
                    {"id": "node_1", "title": "The Hook", "content": "You arrive at the tavern... (Module Not Found)"},
                ]

        elif mode == "B":
            # Remix Mode: Curated from assets
            title = "The Remixed Realm"
            fragments = self._fetch_random_fragments(5)
            nodes = []
            for i, frag in enumerate(fragments):
                nodes.append({
                    "id": f"remix_{i}",
                    "title": f"Encounter: {frag['name']}",
                    "content": f"You encounter a {frag['asset_type']}: {frag['name']}. {frag['raw_text'][:200]}..."
                })
            if not nodes:
                nodes = [{"id": "r1", "title": "Empty World", "content": "The library is silent. No assets found."}]

        else:
            # Full AI Mode
            title = f"AI Story: {prompt[:20]}"
            nodes = [{"id": "c1", "title": "Generative Start", "content": prompt}]

        content = {
            "title": title,
            "nodes": nodes,
            "metadata": {"mode": mode, "prompt": prompt, "source": source_file}
        }

        with sqlite3.connect(self.db_path) as conn:
            conn.execute(
                "INSERT INTO story_outlines (id, campaign_mode, source_file, title, content_json) VALUES (?, ?, ?, ?, ?)",
                (outline_id, mode, source_file, title, json.dumps(content))
            )
        return outline_id

    def link_session(self, session_id: str, outline_id: str):
        """
        Associates a session with an outline.
        """
        with sqlite3.connect(self.db_path) as conn:
            # Ensure session exists or create it
            conn.execute("""
                INSERT OR IGNORE INTO session_state (session_id, outline_id, turn_count, state_json, history_json) 
                VALUES (?, ?, 0, '{}', '[]')
            """, (session_id, outline_id))
            
            # Update if it existed but had no outline or changed
            conn.execute("UPDATE session_state SET outline_id = ? WHERE session_id = ?", (outline_id, session_id))

    def log_turn(self, session_id: str, node_id: str, state_update: Dict):
        """
        RFC-0049: Turn Logging snapshot to DB.
        Merges state_update into the current state and logs the diff to history.
        """
        with sqlite3.connect(self.db_path) as conn:
            conn.row_factory = sqlite3.Row
            session = conn.execute("SELECT * FROM session_state WHERE session_id = ?", (session_id,)).fetchone()
            
            if not session:
                # Initialize session
                # First turn: state_update is the initial state
                conn.execute(
                    "INSERT INTO session_state (session_id, current_node_id, turn_count, state_json, history_json) VALUES (?, ?, ?, ?, ?)",
                    (session_id, node_id, 1, json.dumps(state_update), json.dumps([{"turn": 1, "node": node_id, "diff": state_update}]))
                )
            else:
                new_turn = session['turn_count'] + 1
                try:
                    current_state = json.loads(session['state_json']) if session['state_json'] else {}
                    history = json.loads(session['history_json'])
                except:
                    current_state = {}
                    history = []
                
                # Merge update into current state
                current_state.update(state_update)
                
                # Log the diff
                history.append({"turn": new_turn, "node": node_id, "diff": state_update})
                
                conn.execute(
                    "UPDATE session_state SET current_node_id = ?, turn_count = ?, state_json = ?, history_json = ?, updated_at = CURRENT_TIMESTAMP WHERE session_id = ?",
                    (node_id, new_turn, json.dumps(current_state), json.dumps(history), session_id)
                )
        return True

    def get_rehydration_data(self, session_id: str):
        """
        Memory Cycling Rehydration: Reloads outline and latest state.
        """
        with sqlite3.connect(self.db_path) as conn:
            conn.row_factory = sqlite3.Row
            session = conn.execute("""
                SELECT s.*, o.content_json as outline_content 
                FROM session_state s
                JOIN story_outlines o ON s.outline_id = o.id
                WHERE s.session_id = ?
            """, (session_id,)).fetchone()
            
            if not session: return None
            return dict(session)

if __name__ == "__main__":
    # Test Init
    engine = OutlineEngine(os.path.expanduser("~/.dnd_dm/cortex.db"))
    print("Outline Engine Initialized.")
