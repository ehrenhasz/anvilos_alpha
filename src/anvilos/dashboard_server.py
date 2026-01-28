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
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)
PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
CORTEX_DB = os.path.join(PROJECT_ROOT, "..", "..", "data", "cortex.db")
INTERCEPTOR_WS_URL = "ws://localhost:8001/ws/shell"
API_BACKEND_URL = "http://localhost:5000"
@app.get("/api/chat/history")
def get_chat_history():
    if not os.path.exists(CORTEX_DB): return []
    try:
        with sqlite3.connect(CORTEX_DB) as conn:
            conn.row_factory = sqlite3.Row
            inbox = conn.execute("SELECT id, message, timestamp, 'user' as role FROM chat_inbox").fetchall()
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
            async def browser_to_interceptor():
                try:
                    while True:
                        data = await websocket.receive_text()
                        await interceptor_ws.send(data)
                except WebSocketDisconnect:
                    pass
                except Exception as e:
                    print(f"Proxy Browser-> Interceptor Error: {e}")
            async def interceptor_to_browser():
                try:
                    while True:
                        data = await interceptor_ws.recv()
                        await websocket.send_text(data)
                except Exception as e:
                    print(f"Proxy Interceptor->Browser Error: {e}")
            await asyncio.gather(
                browser_to_interceptor(),
                interceptor_to_browser()
            )
    except Exception as e:
        print(f"Interceptor Bridge Connection Failed: {e}")
        await websocket.send_text(f"\r\n\x1b[1;31mINTERCEPTOR OFFLINE: {e}\x1b[0m\r\n")
        await websocket.close()
DIST_DIR = os.path.join(PROJECT_ROOT, "interface_v2", "dist")
if os.path.exists(DIST_DIR):
    app.mount("/", StaticFiles(directory=DIST_DIR, html=True), name="static")
if __name__ == "__main__":
    import uvicorn
    print("VERSION: INTERCEPTOR_PROXY_V1")
    uvicorn.run(app, host="0.0.0.0", port=8000)