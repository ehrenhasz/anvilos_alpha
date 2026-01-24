#!/usr/bin/env python3
import time
import sqlite3
import json
import subprocess
import os
import sys
import select
from collections import deque
from datetime import datetime
import shutil

# VISUALS
import rich
import rich.live
import rich.layout
import rich.panel
import rich.table
import rich.align
import rich.text
import rich.box

# METRICS
try:
    import psutil
    HAS_PSUTIL = True
except ImportError:
    HAS_PSUTIL = False

# --- CONFIGURATION ---
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
DB_PATH = "/var/lib/anvilos/db/cortex.db"
LOG_FILE = os.path.join(PROJECT_ROOT, "ext", "forge.log")
SERVICE_NAME = "bigiron"

# --- THEME ---
C_OK   = "#50fa7b"   # Green
C_RUN  = "#bd93f9"   # Purple
C_QUE  = "#8be9fd"   # Cyan/Blue
C_ERR  = "#ff5555"   # Red
C_WARN = "#f1fa8c"   # Yellow
C_LABEL = "#6272a4"  # Grey-Blue
C_BORDER = "#44475a" # Dark Grey
C_TEXT = "#f8f8f2"   # White

# Legacy Mappings for existing panels
C_PRIMARY = C_OK
C_SECONDARY = C_RUN
C_ACCENT = C_QUE

# --- METRICS ---
class NetMonitor:
    def __init__(self):
        self.last_sent = 0
        self.last_recv = 0
        self.last_time = time.time()
        self.up = 0.0
        self.down = 0.0
        if HAS_PSUTIL:
            try:
                n = psutil.net_io_counters()
                self.last_sent, self.last_recv = n.bytes_sent, n.bytes_recv
            except: pass

    def update(self):
        if not HAS_PSUTIL: return
        now = time.time()
        try:
            n = psutil.net_io_counters()
            dt = now - self.last_time
            if dt >= 1.0:
                self.up = (n.bytes_sent - self.last_sent) / 1024 / 1024 / dt
                self.down = (n.bytes_recv - self.last_recv) / 1024 / 1024 / dt
                self.last_sent, self.last_recv = n.bytes_sent, n.bytes_recv
                self.last_time = now
        except: pass

net_mon = NetMonitor()

def get_disk():
    try:
        u = shutil.disk_usage("/")
        return f"{u.used // (1024**3)}/{u.total // (1024**3)}G"
    except: return "N/A"

# --- DBREADER ---
class CortexReader:
    def __init__(self):
        self.stats = {"proc": 0, "queue": 0, "done": 0, "fail": 0}
        self.cards = []
        self.agents = []

    def purge(self):
        try:
            with sqlite3.connect(DB_PATH) as conn:
                conn.execute("DELETE FROM jobs WHERE status IN ('COMPLETE', 'FAILED')")
        except: pass

    def scan(self):
        if not os.path.exists(DB_PATH): return
        try:
            conn = sqlite3.connect(DB_PATH)
            conn.row_factory = sqlite3.Row
            cur = conn.cursor()

            # JOB STATS
            cur.execute("SELECT status, COUNT(*) FROM jobs GROUP BY status")
            self.stats = {"proc": 0, "queue": 0, "done": 0, "fail": 0}
            for r in cur.fetchall():
                s = r[0].lower()
                c = r[1]
                if s == 'processing': self.stats['proc'] = c
                elif s in ['pending','paused','assigned']: self.stats['queue'] += c
                elif s == 'complete': self.stats['done'] = c
                elif s == 'failed': self.stats['fail'] = c

            # JOB FEED
            cur.execute("""
                SELECT correlation_id, priority, cost_center, status, payload 
                FROM jobs ORDER BY updated_at DESC LIMIT 15
            """)
            self.cards = [dict(r) for r in cur.fetchall()]

            # AGENTS
            cur.execute("SELECT agent_id, coding_id, status, updated_at FROM agents ORDER BY agent_id")
            self.agents = [dict(r) for r in cur.fetchall()]
            
            conn.close()
        except: pass

# --- LAYOUT ---
def make_layout():
    layout = rich.layout.Layout()
    layout.split(
        rich.layout.Layout(name="header", size=1),
        rich.layout.Layout(name="upper", ratio=1),
        rich.layout.Layout(name="lower", ratio=1),
        rich.layout.Layout(name="footer", size=1)
    )
    # Upper: Feed
    layout["upper"].update(rich.panel.Panel("", title="FEED"))
    
    # Lower: System | Agents | Logs
    layout["lower"].split_row(
        rich.layout.Layout(name="sys", ratio=1),
        rich.layout.Layout(name="agents", ratio=1),
        rich.layout.Layout(name="logs", ratio=2)
    )
    return layout

# --- PANELS ---
def panel_header():
    t = rich.text.Text(f" ANVIL OS // CORTEX CONNECTED // {datetime.now().strftime('%H:%M:%S')} ", style="bold white on #44475a", justify="center")
    return t

def panel_feed(reader):

    table = rich.table.Table(box=None, expand=True, show_header=False, padding=(0,1))

    table.add_column("ID", width=8, style=C_LABEL)

    table.add_column("ST", width=10)

    table.add_column("PAYLOAD")

    

    for c in reader.cards:

        st = c['status'][:4].upper()

        col = C_LABEL

        if st in ["COMP", "DONE"]: col = C_OK

        elif st in ["FAIL", "ERR "]: col = C_ERR

        elif st in ["PROC", "RUN ", "ASSI"]: col = C_RUN

        elif st in ["PEND", "QUE ", "WAIT"]: col = C_QUE

        

        try:

            pl = json.loads(c['payload']).get('description', 'Data')[:50]

        except: pl = "Raw Data"

        

        table.add_row(c['correlation_id'][:8], f"[{col}]{st}[/]", f"[{col}]{pl}[/]")

    

    sub = f"[{C_RUN}]RUN:{reader.stats['proc']}[/] | [{C_QUE}]QUE:{reader.stats['queue']}[/] | [{C_OK}]OK:{reader.stats['done']}[/] | [{C_ERR}]ERR:{reader.stats['fail']}[/]"

    return rich.panel.Panel(table, title=f"[bold {C_RUN}]CORTEX FEED[/]", subtitle=sub, border_style=C_BORDER)
    
    
    
def panel_system():



    net_mon.update()



    table = rich.table.Table(box=None, expand=True, show_header=False)



    



    if HAS_PSUTIL:



        cpu = psutil.cpu_percent()



        ram = psutil.virtual_memory().percent



        table.add_row("CPU", f"[{C_PRIMARY if cpu<50 else C_ERR}]{cpu}%[/]")



        table.add_row("RAM", f"[{C_ACCENT}]{ram}%[/]")



    



    table.add_row("DSK", get_disk())



    table.add_row("NET", f"^{net_mon.up:.1f} v{net_mon.down:.1f}")



    



    return rich.panel.Panel(table, title=f"[bold {C_SECONDARY}]SYSTEM[/]", border_style=C_BORDER)

def panel_agents(reader):
    table = rich.table.Table(box=None, expand=True, show_header=False)
    for a in reader.agents:
        code = a['coding_id'] or "UNK"
        st = a['status']
        col = C_PRIMARY if st=="ONLINE" else C_ERR
        table.add_row(code, f"[{col}]{st}[/]")
        
    return rich.panel.Panel(table, title=f"[bold {C_PRIMARY}]AGENTS[/]", border_style=C_BORDER)

class LogTail:
    def __init__(self, log_path, title):
        self.log_path = log_path
        self.title = title
        self.lines = deque(maxlen=15)

    def update(self):
        if not os.path.exists(self.log_path): return
        try:
            with open(self.log_path, 'r') as f:
                for line in f.readlines()[-15:]:
                    try:
                        data = json.loads(line)
                        msg = data.get("data", "")
                        ts = data.get("ts", "")
                        try:
                            ts_short = datetime.fromisoformat(ts).strftime("%H:%M:%S")
                        except: ts_short = "??"
                        clean = f"{ts_short} | {msg}"
                    except:
                        clean = line.strip()
                    
                    # Deduplicate based on text content (excluding color tags if any)
                    if clean not in [l.split(']', 1)[-1] if ']' in l else l for l in self.lines]:
                        col = C_TEXT
                        if "SUCCESS" in clean or "COMPLETED" in clean: col = C_OK
                        elif "FAILED" in clean or "ERROR" in clean or "CRASH" in clean: col = C_ERR
                        elif "STARTING" in clean or "RUNNING" in clean or "USER:" in clean: col = C_RUN
                        elif "PENDING" in clean: col = C_QUE
                        
                        self.lines.append(f"[{col}]{clean}[/]")
        except: pass

    def get(self):
        t = rich.text.Text.from_markup("\n".join(self.lines))
        return rich.panel.Panel(t, title=self.title, border_style=C_BORDER)

# --- MAIN ---
def main():
    layout = make_layout()
    reader = CortexReader()
    forge_logs = LogTail(LOG_FILE, "SYSTEM LOGS")
    ls_logs = LogTail(os.path.join(PROJECT_ROOT, "ext", "ladysmith.log"), "LADYSMITH")
    
    # Split logs layout
    layout["logs"].split_column(
        rich.layout.Layout(name="forge_logs"),
        rich.layout.Layout(name="ls_logs")
    )
    
    with rich.live.Live(layout, refresh_per_second=4, screen=True):
        while True:
            reader.scan()
            forge_logs.update()
            ls_logs.update()
            
            layout["header"].update(panel_header())
            layout["upper"].update(panel_feed(reader))
            layout["sys"].update(panel_system())
            layout["agents"].update(panel_agents(reader))
            layout["forge_logs"].update(forge_logs.get())
            layout["ls_logs"].update(ls_logs.get())
            layout["footer"].update(rich.align.Align.center("[Q] Quit  [C] Clear Jobs"))
            
            # Input handling simplified
            if select.select([sys.stdin], [], [], 0.1)[0]:
                if sys.stdin.read(1).lower() == 'q': break
                
if __name__ == "__main__":
    main()