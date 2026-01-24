import os
import sqlite3
import json
import asyncio
import websockets
import httpx
from fastapi import FastAPI, WebSocket, Request, WebSocketDisconnect
from fastapi.responses import Response
from fastapi.staticfiles import StaticFiles
from fastapi.middleware.cors import CORSMiddleware

app = FastAPI()

# --- INITIALIZE ENGINES ---
from outline_engine import OutlineEngine
CORTEX_DB = "/var/lib/anvilos/db/cortex.db"
outline_engine = OutlineEngine(CORTEX_DB)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# --- OUTLINE ENGINE ENDPOINTS ---

@app.post("/api/campaign/generate")
async def generate_campaign(request: Request):
    data = await request.json()
    mode = data.get("mode", "C")
    prompt = data.get("prompt", "")
    source_file = data.get("source_file", "")
    outline_id = outline_engine.generate_outline(mode, prompt, source_file)
    return {"outline_id": outline_id}

@app.post("/api/session/link")
async def link_session(request: Request):
    data = await request.json()
    session_id = data.get("session_id")
    outline_id = data.get("outline_id")
    outline_engine.link_session(session_id, outline_id)
    return {"status": "linked"}

@app.post("/api/session/log_turn")
async def log_turn(request: Request):
    data = await request.json()
    session_id = data.get("session_id")
    node_id = data.get("node_id")
    state_update = data.get("state_update", {})
    outline_engine.log_turn(session_id, node_id, state_update)
    return {"status": "logged"}

@app.get("/api/session/rehydrate/{session_id}")
async def rehydrate_session(session_id: str):
    data = outline_engine.get_rehydration_data(session_id)
    if not data:
        return Response(status_code=404, content="Session not found")
    return data

@app.post("/api/character/save")
async def save_character(request: Request):
    data = await request.json()
    char_id = data.get("id")
    with sqlite3.connect(CORTEX_DB) as conn:
        conn.execute("""
            CREATE TABLE IF NOT EXISTS characters (
                id TEXT PRIMARY KEY,
                name TEXT,
                data_json TEXT,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        """)
        conn.execute(
            "INSERT OR REPLACE INTO characters (id, name, data_json) VALUES (?, ?, ?)",
            (char_id, data.get("name"), json.dumps(data))
        )
    return {"status": "saved", "id": char_id}

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
# Hardcoded path to ensure consistency
CORTEX_DB = "/var/lib/anvilos/db/cortex.db"
APP_DB = "/var/lib/anvilos/db/dnd_dm.db"
INTERCEPTOR_WS_URL = "ws://localhost:8001/ws/shell"
API_BACKEND_URL = "http://localhost:5000"

@app.get("/api/queue")
def get_queue():
    if not os.path.exists(APP_DB): return []
    try:
        with sqlite3.connect(APP_DB) as conn:
            conn.row_factory = sqlite3.Row
            cursor = conn.cursor()
            cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='library_index'")
            if not cursor.fetchone(): return []
            rows = conn.execute("SELECT filename, metadata FROM library_index ORDER BY rowid DESC LIMIT 50").fetchall()
            return [{"id": r["filename"], "filename": r["filename"], "status": "COMPLETED", "meta": r["metadata"]} for r in rows]
    except: return []

@app.get("/api/chat/history")
def get_chat_history():
    if not os.path.exists(CORTEX_DB): return []
    try:
        with sqlite3.connect(CORTEX_DB) as conn:
            conn.row_factory = sqlite3.Row
            # Fetch User Messages
            inbox = conn.execute("SELECT id, message, timestamp, 'user' as role FROM chat_inbox").fetchall()
            # Fetch Agent Responses
            outbox = conn.execute("SELECT id, message, timestamp, 'model' as role FROM chat_outbox").fetchall()
            
            history = [dict(row) for row in inbox] + [dict(row) for row in outbox]
            history.sort(key=lambda x: x['timestamp'])
            return history
    except: return []

@app.post("/api/chat/send")
async def send_message(request: Request):
    data = await request.json()
    message = data.get("message")
    if not message: return {"error": "No message provided"}
    
    try:
        with sqlite3.connect(CORTEX_DB) as conn:
            conn.execute("INSERT INTO chat_inbox (agent_id, message, status) VALUES (?, ?, ?)", ("aimeat", message, "PENDING"))
            conn.commit()
        return {"status": "SENT"}
    except Exception as e:
        return {"error": str(e)}

@app.get("/api/library/fragments")
def get_fragments(type: str = None, limit: int = 100):
    print(f"DEBUG: Connecting to {APP_DB}")
    if not os.path.exists(APP_DB): 
        print("DEBUG: DB NOT FOUND")
        return []
    try:
        with sqlite3.connect(APP_DB) as conn:
            conn.row_factory = sqlite3.Row
            query = "SELECT id, asset_type, name, content_json, raw_text FROM fragments"
            params = []
            if type and type != "all":
                query += " WHERE asset_type = ?"
                params.append(type)
            query += " ORDER BY created_at DESC LIMIT ?"
            params.append(limit)
            
            rows = conn.execute(query, tuple(params)).fetchall()
            print(f"DEBUG: Fetched {len(rows)} rows for type={type}")
            return [dict(r) for r in rows]
    except Exception as e:
        print(f"DEBUG: Error {e}")
        return {"error": str(e)}

@app.api_route("/api/{path_name:path}", methods=["GET", "POST", "PUT", "DELETE"])
async def proxy_api(path_name: str, request: Request):
    """
    Proxies all other /api requests to the Flask Backend on Port 5000.
    """
    async with httpx.AsyncClient() as client:
        url = f"{API_BACKEND_URL}/api/{path_name}"
        query = request.url.query
        if query:
            url += f"?{query}"
            
        try:
            # Exclude Host header to avoid confusion at the destination
            headers = dict(request.headers)
            headers.pop("host", None)
            headers.pop("content-length", None) # Let httpx handle this
            
            rp_req = client.build_request(
                request.method, 
                url, 
                headers=headers, 
                content=await request.body()
            )
            rp_resp = await client.send(rp_req)
            
            return Response(
                content=rp_resp.content, 
                status_code=rp_resp.status_code, 
                headers=dict(rp_resp.headers)
            )
        except Exception as e:
            print(f"Proxy Error: {e}")
            return Response(content=f'{{"error": "API Proxy Failed: {str(e)}"}}', status_code=502, media_type="application/json")

@app.websocket("/ws/shell")
async def websocket_shell_proxy(websocket: WebSocket):
    """
    RFC-0051 PROXY: Bridges the browser to the Interceptor Bridge on port 8001.
    """
    await websocket.accept()
    
    try:
        async with websockets.connect(INTERCEPTOR_WS_URL) as interceptor_ws:
            
            # Forward Browser -> Interceptor
            async def browser_to_interceptor():
                try:
                    while True:
                        data = await websocket.receive_text()
                        await interceptor_ws.send(data)
                except WebSocketDisconnect:
                    pass
                except Exception as e:
                    print(f"Proxy Browser-> Interceptor Error: {e}")

            # Forward Interceptor -> Browser
            async def interceptor_to_browser():
                try:
                    while True:
                        data = await interceptor_ws.recv()
                        await websocket.send_text(data)
                except Exception as e:
                    print(f"Proxy Interceptor->Browser Error: {e}")

            # Run both as tasks
            await asyncio.gather(
                browser_to_interceptor(),
                interceptor_to_browser()
            )
            
    except Exception as e:
        print(f"Interceptor Bridge Connection Failed: {e}")
        await websocket.send_text(f"\r\n\x1b[1;31mINTERCEPTOR OFFLINE: {e}\x1b[0m\r\n")
        await websocket.close()

# Serve Static Files
DIST_DIR = os.path.join(PROJECT_ROOT, "interface_v2", "dist")
if os.path.exists(DIST_DIR):
    app.mount("/", StaticFiles(directory=DIST_DIR, html=True), name="static")

if __name__ == "__main__":
    import uvicorn
    print("VERSION: INTERCEPTOR_PROXY_V1")
    uvicorn.run(app, host="0.0.0.0", port=8000)