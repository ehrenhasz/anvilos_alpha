from flask import Flask, jsonify, request, send_from_directory
from flask_cors import CORS
import sqlite3
import os
import sys

app = Flask(__name__)
CORS(app)

# Path to the Cortex DB (RFC-0049: Local Cortex)
DB_PATH = os.path.expanduser("~/.dnd_dm/cortex.db")
# LIBRARY_DIR = os.path.expanduser("~/Documents/DriveThruRPG") 
# Point to the organized library in the repo
LIBRARY_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'DND_Library'))

def get_db_connection():
    if not os.path.exists(DB_PATH):
        return None
    try:
        conn = sqlite3.connect(DB_PATH)
        conn.row_factory = sqlite3.Row
        return conn
    except:
        return None

@app.route('/api/status', methods=['GET'])
def get_status():
    return jsonify({
        "status": "ONLINE",
        "agent": "dnd_dm",
        "db_path": DB_PATH,
        "db_exists": os.path.exists(DB_PATH)
    })

@app.route('/api/jobs', methods=['GET'])
def get_jobs():
    limit = request.args.get('limit', 20)
    conn = get_db_connection()
    if not conn:
        return jsonify([])
    try:
        jobs = conn.execute('SELECT * FROM jobs ORDER BY updated_at DESC LIMIT ?', (limit,)).fetchall()
        conn.close()
        return jsonify([dict(job) for job in jobs])
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/logs', methods=['GET'])
def get_logs():
    limit = request.args.get('limit', 50)
    conn = get_db_connection()
    if not conn:
        return jsonify([])
    try:
        # Check if table exists first
        cursor = conn.cursor()
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='experience_log';")
        if not cursor.fetchone():
            return jsonify([])
            
        logs = conn.execute('SELECT * FROM experience_log ORDER BY timestamp DESC LIMIT ?', (limit,)).fetchall()
        conn.close()
        return jsonify([dict(log) for log in logs])
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/library', methods=['GET'])
def get_library():
    """Fetches the library index from Cortex DB with metadata."""
    conn = get_db_connection()
    if not conn:
        return jsonify([])
    try:
        # Ensure table exists
        cursor = conn.cursor()
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='library_index';")
        if not cursor.fetchone():
            return jsonify([])

        rows = conn.execute('SELECT id, filename, path, tags, metadata, butchered_at FROM library_index ORDER BY id DESC').fetchall()
        conn.close()
        
        library_items = []
        import json
        for row in rows:
            # Parse metadata JSON string if it exists
            meta = {}
            if row['metadata']:
                try:
                    meta = json.loads(row['metadata'])
                    if isinstance(meta, list) and len(meta) > 0:
                        meta = meta[0]
                    
                    # Map keys for frontend compatibility
                    if 'system' in meta and 'system_or_genre' not in meta:
                        meta['system_or_genre'] = meta['system']
                    if 'type' in meta and 'product_type' not in meta:
                        meta['product_type'] = meta['type']
                except:
                    meta = {}
            
            # Calculate relative path for serving
            # stored path is absolute, LIBRARY_DIR is absolute
            try:
                rel_path = os.path.relpath(row['path'], LIBRARY_DIR)
            except ValueError:
                # Fallback if path is weird
                rel_path = row['filename']

            library_items.append({
                "id": row['id'],
                "name": row['filename'],
                "rel_path": rel_path,
                "tags": row['tags'],
                "metadata": meta,
                "butchered_at": row['butchered_at']
            })
            
        return jsonify(library_items)
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/fragments', methods=['GET'])
def get_fragments():
    """Fetches atomized fragments, optionally filtered by type."""
    asset_type = request.args.get('type')
    limit = request.args.get('limit', 100)
    
    conn = get_db_connection()
    if not conn:
        return jsonify([])
    
    try:
        query = "SELECT id, asset_type, name, content_json FROM fragments"
        params = []
        
        if asset_type:
            query += " WHERE asset_type = ?"
            params.append(asset_type)
            
        query += " ORDER BY name ASC LIMIT ?"
        params.append(limit)
        
        rows = conn.execute(query, tuple(params)).fetchall()
        conn.close()
        
        import json
        fragments = []
        for row in rows:
            try:
                content = json.loads(row['content_json'])
            except:
                content = {}
                
            fragments.append({
                "id": row['id'],
                "type": row['asset_type'],
                "name": row['name'],
                "data": content
            })
            
        return jsonify(fragments)
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/view/<path:filename>')
def view_pdf(filename):
    """Serves a PDF file from the library."""
    return send_from_directory(LIBRARY_DIR, filename)

@app.route('/api/agents', methods=['GET'])
def get_agents():
    try:
        conn = get_db_connection()
        if not conn:
            return jsonify([])
        agents = conn.execute('SELECT * FROM agents').fetchall()
        conn.close()
        return jsonify([dict(agent) for agent in agents])
    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    port = int(os.environ.get('PORT', 5000))
    app.run(host='0.0.0.0', port=port)