import os
import sys
import json
import asyncio
import pty
import select
import struct
import fcntl
import termios
import subprocess
import signal
import sqlite3
import datetime
from fastapi import FastAPI, WebSocket
from fastapi.middleware.cors import CORSMiddleware
import uvicorn
from interpretation import Interpreter

# RFC-0051: THE INTERCEPTOR
# This service implements the managed gateway for the Gemini CLI V2.

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.get("/")
def read_root():
    return {"status": "ONLINE", "service": "Interceptor Bridge", "protocol": "RFC-0051"}

@app.get("/api/status")
def get_status():
    return {"status": "ACTIVE", "mode": "MEDIATED", "db_path": DB_PATH}

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO_ROOT = os.path.dirname(PROJECT_ROOT)
DB_PATH = "/var/lib/anvilos/db/cortex.db"

class CollarShell:
    """
    Manages the PTY interaction with Interceptor hooks and Cortex logging.
    """
    def __init__(self, session_id: str):
        self.session_id = session_id
        self.master_fd, self.slave_fd = pty.openpty()
        self.process = subprocess.Popen(
            ["/bin/bash"],
            preexec_fn=os.setsid,
            stdin=self.slave_fd,
            stdout=self.slave_fd,
            stderr=self.slave_fd,
            cwd=REPO_ROOT,
            env={**os.environ, "TERM": "xterm-256color", "COLLAR_MODE": "MEDIATED"}
        )
        os.close(self.slave_fd)
        self.mode = "DIRECT"
        self.interpreter = Interpreter()
        self.input_buffer = ""
        self._init_db()

    def _init_db(self):
        try:
            os.makedirs(os.path.dirname(DB_PATH), exist_ok=True)
            conn = sqlite3.connect(DB_PATH)
            conn.execute("""
                CREATE TABLE IF NOT EXISTS terminal_events (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    session_id TEXT,
                    direction TEXT,
                    payload TEXT,
                    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
                )
            """)
            conn.commit()
            conn.close()
        except Exception as e:
            print(f"DB Init Error: {e}")

    def log_event(self, direction: str, payload: str):
        """Logs a shell event to the Cortex."""
        try:
            conn = sqlite3.connect(DB_PATH)
            conn.execute("INSERT INTO terminal_events (session_id, direction, payload) VALUES (?, ?, ?)",
                         (self.session_id, direction, payload))
            conn.commit()
            conn.close()
        except: pass

    def write(self, data: str):
        """Writes data to the PTY, intercepted and logged."""
        self.log_event("INPUT", data)
        
        # RFC-0051: MODE SWITCHING & INTERPRETATION
        # Check for magic commands in buffer if not passed directly
        
        if self.mode == "MEDIATED":
            # Echo back to user so they see what they type (local echo simulation for the buffer)
            # Actual PTY echo might be confusing, so usually we let the PTY handle echo if we pass it through.
            # But here we are INTERCEPTING. We must handle echo if we don't write to PTY.
            # For simplicity, we'll write to PTY only on newline, but we need to give feedback.
            # This is tricky with xterm.js.
            # STRATEGY: We will just accumulate buffer. The user won't see characters until we process? 
            # No, that's bad UX.
            # Better: Write to PTY as comment? No.
            # We'll just pass through everything to PTY so it echoes, but if we detect a newline, 
            # we capture the LAST command line and deciding if we want to "Execute" or "Interpret".
            # BUT: If we pass to PTY, it executes!
            # So in MEDIATED mode, we CANNOT pass to PTY.
            # We must echo manually back to the websocket (by writing to master_fd? No, that goes to shell input).
            # We must write to the websocket directly? But this method only writes to PTY.
            
            # Let's simplify: Toggle Mode via special command.
            pass

        # Handle Magic Commands
        if data == '\r': # Enter
            if self.input_buffer.strip() == ";;mode ai":
                self.mode = "MEDIATED"
                os.write(self.master_fd, b"\r\n\x1b[1;35m[SYSTEM] MODE SWITCH: MEDIATED (AI)\x1b[0m\r\n")
                self.input_buffer = ""
                return
            elif self.input_buffer.strip() == ";;mode direct":
                self.mode = "DIRECT"
                os.write(self.master_fd, b"\r\n\x1b[1;32m[SYSTEM] MODE SWITCH: DIRECT (BASH)\x1b[0m\r\n")
                self.input_buffer = ""
                return
            
            if self.mode == "MEDIATED":
                # Intercept!
                user_req = self.input_buffer
                os.write(self.master_fd, b"\r\n\x1b[1;33m[AIMEAT] Thinking...\x1b[0m\r\n")
                
                cmd = self.interpreter.interpret(user_req)
                
                os.write(self.master_fd, f"\x1b[1;36m> {cmd}\x1b[0m\r\n".encode())
                os.write(self.master_fd, cmd.encode() + b"\r") # Execute interpreted command
                self.input_buffer = ""
                return

            self.input_buffer = ""
        
        elif data == '\x7f': # Backspace
            self.input_buffer = self.input_buffer[:-1]
        else:
            self.input_buffer += data

        # Pass through to PTY (Echo handled by PTY)
        # In MEDIATED mode, we might want to SUPPRESS PTY echo and do it manually, 
        # but for now let's allow "Hybrid" mode where you type in shell, and if you are in AI mode, 
        # hitting ENTER triggers AI instead of Bash. 
        # PROBLEM: Bash will also receive the characters!
        # SOLUTION: In MEDIATED mode, DO NOT write to PTY until Enter.
        # We need to manually echo characters to the user.
        
        if self.mode == "MEDIATED":
            # Manual Echo
            if data == '\r': pass # Handled above
            elif data == '\x7f': # Backspace
                 # ANSI sequence to move back and clear char
                 os.write(self.master_fd, b"\b \b")
            else:
                 os.write(self.master_fd, data.encode())
        else:
            os.write(self.master_fd, data.encode())

    def resize(self, rows: int, cols: int):
        self.log_event("RESIZE", f"{cols}x{rows}")
        winsize = struct.pack("HHHH", rows, cols, 0, 0)
        fcntl.ioctl(self.master_fd, termios.TIOCSWINSZ, winsize)
        try:
            os.killpg(os.getpgid(self.process.pid), signal.SIGWINCH)
        except: pass

    def close(self):
        os.close(self.master_fd)
        if self.process.poll() is None:
            self.process.terminate()
            self.process.wait()

@app.websocket("/ws/shell")
async def websocket_shell(websocket: WebSocket):
    await websocket.accept()
    
    import uuid
    session_id = str(uuid.uuid4())
    shell = CollarShell(session_id)
    
    async def pty_reader():
        while True:
            await asyncio.sleep(0.01)
            try:
                r, _, _ = select.select([shell.master_fd], [], [], 0)
                if shell.master_fd in r:
                    output = os.read(shell.master_fd, 4096)
                    if output:
                        text = output.decode('utf-8', errors='ignore')
                        # Log output for Algorithmic Improvement (RFC-0051)
                        shell.log_event("OUTPUT", text)
                        await websocket.send_text(text)
                    else: break
            except Exception: break

    async def ws_reader():
        try:
            while True:
                data_text = await websocket.receive_text()
                try:
                    msg = json.loads(data_text)
                    if msg['type'] == 'input':
                        shell.write(msg['data'])
                    elif msg['type'] == 'resize':
                        shell.resize(int(msg['rows']), int(msg['cols']))
                except Exception: pass
        except Exception: pass

    tasks = [asyncio.create_task(pty_reader()), asyncio.create_task(ws_reader())]
    try:
        await asyncio.wait(tasks, return_when=asyncio.FIRST_COMPLETED)
    finally:
        for t in tasks: t.cancel()
        shell.close()

if __name__ == "__main__":
    print("Interceptor Bridge Online on Port 8001")
    uvicorn.run(app, host="0.0.0.0", port=8001)
