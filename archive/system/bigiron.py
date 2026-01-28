#!/usr/bin/env python3
import time
import json
import subprocess
import os
from collections import deque
from datetime import datetime
import rich
import rich.live
import rich.layout
import rich.panel
import rich.table
import rich.console
import rich.text
import rich.box
import rich.align

# BTOP DEFAULT FIDELITY THEME (Sleek/Dense)
C_PRIMARY = "#50fa7b"   # Green
C_SECONDARY = "#8be9fd" # Cyan
C_ACCENT = "#bd93f9"    # Purple
C_WARN = "#f1fa8c"      # Yellow
C_ERR = "#ff5555"       # Red
C_LABEL = "#6272a4"     # Grey
C_BORDER = "#44475a"    # Muted
C_TEXT = "#f8f8f2"

# Get the absolute path to the project root
# The script is in system/, so we go up one level
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

QUEUE_FILE = os.path.join(PROJECT_ROOT, "runtime", "card_queue.json")
LOG_FILE = os.path.join(PROJECT_ROOT, "ext", "forge.log")
SERVICE_NAME = "titanium_warden"

def make_layout():
    # Avoid shadowing 'Layout' class with 'layout' variable
    root_layout = rich.layout.Layout(name="root")
    
    root_layout.split(
        rich.layout.Layout(name="header", size=1),
        rich.layout.Layout(name="cards", ratio=1),  # UPPER: Card Reader
        rich.layout.Layout(name="btop", ratio=1),   # LOWER: Btop
        rich.layout.Layout(name="footer", size=1),
    )
    
    # Btop Lower Half Layout
    root_layout["btop"].split_row(
        rich.layout.Layout(name="left", ratio=1),
        rich.layout.Layout(name="right", ratio=2),
    )
    root_layout["left"].split(
        rich.layout.Layout(name="cpu_mem", ratio=1),
        rich.layout.Layout(name="net", size=6),
    )
    root_layout["right"].split(
        rich.layout.Layout(name="proc", ratio=1), 
    )
    return root_layout

def make_graph(value, color, width=15):
    filled = int((min(max(value, 0), 100) / 100) * width)
    return f"[{color}]" + "█" * filled + "[/]" + "[#282a36]" + "█" * (width - filled) + "[/]"

class Header:
    def __rich__(self):
        t = rich.text.Text()
        t.append(" btop ", style="reverse bold #8be9fd")
        t.append(f" v1.4.6 ", style="bold white")
        t.append(" host: ", style=C_LABEL)
        t.append("anvilos ", style=C_PRIMARY)
        t.append(" up: ", style=C_LABEL)
        t.append("00:42:12 ", style=C_WARN)
        t.append(" " * 10)
        t.append(datetime.now().strftime("%H:%M:%S"), style="bold white")
        return rich.align.Align.center(t)

class Footer:
    def __rich__(self):
        t = rich.text.Text()
        t.append(" f1 ", style="reverse")
        t.append(" Help ", style=C_TEXT)
        t.append(" f2 ", style="reverse")
        t.append(" Options ", style=C_TEXT)
        t.append(" q ", style="reverse #ff5555")
        t.append(" Quit ", style=C_TEXT)
        t.append(" " * 5)
        t.append(" bigiron_session: ", style=C_LABEL)
        t.append("SECURE_OMEGA", style=C_ACCENT)
        return rich.align.Align.center(t)

def cpu_mem_panel():
    table = rich.table.Table.grid(expand=True)
    table.add_row(f"[{C_LABEL}]CPU usage[/]", rich.align.Align.right(f"[{C_PRIMARY}]4%[/]"))
    table.add_row(make_graph(4, C_PRIMARY, width=30))
    table.add_row("")
    table.add_row(f"[{C_LABEL}]Mem used[/]", rich.align.Align.right(f"[{C_SECONDARY}]2.4GB[/]"))
    table.add_row(make_graph(15, C_SECONDARY, width=30))
    return rich.panel.Panel(table, title="[bold #bd93f9]system[/]", border_style=C_BORDER, box=rich.box.SQUARE, padding=(0,1))

def net_panel():
    try:
        res = subprocess.run(["systemctl", "is-active", SERVICE_NAME], capture_output=True, text=True)
        status = res.stdout.strip()
    except: status = "unknown"
    table = rich.table.Table.grid(expand=True)
    table.add_row(f"[{C_LABEL}]net_warden:[/]  [{C_PRIMARY if status=='active' else C_ERR}]{status.upper()}[/]")
    table.add_row(f"[{C_LABEL}]pqc_link:  [/]  [{C_PRIMARY}]STABLE[/]")
    return rich.panel.Panel(table, title="[bold #50fa7b]net[/]", border_style=C_BORDER, box=rich.box.SQUARE, padding=(0,1))

def proc_panel(watcher):
    # Render raw logs from the watcher buffer
    log_text = rich.text.Text()
    # Iterate reversed to show newest lines at the top
    for line in reversed(watcher.buffer):
        # Simple syntax highlighting for logs
        style = C_TEXT
        if "ERROR" in line or "FAILURE" in line:
            style = C_ERR
        elif "SUCCESS" in line or "CONFIRMED" in line:
            style = C_PRIMARY
        elif "PROCESSING" in line:
            style = C_ACCENT
        elif ">>" in line:
            style = C_WARN
            
        log_text.append(line + "\n", style=style)
            
    return rich.panel.Panel(log_text, title="[bold #f1fa8c]proc (raw_tty)[/]", border_style=C_BORDER, box=rich.box.SQUARE, padding=(0,1))

def cards_panel():
    table = rich.table.Table(expand=True, box=None, padding=(0, 1), show_header=True, header_style=C_LABEL)
    table.add_column("ID", style=C_SECONDARY, width=10)
    table.add_column("STATUS", width=10)
    table.add_column("TASK")
    
    count_proc = 0
    count_queue = 0 # pending
    count_done = 0
    count_fail = 0
    
    try:
        with open(QUEUE_FILE, "r") as f:
            cards = json.load(f)
            
            # Count statistics (before slicing)
            for c in cards:
                s = c.get("status", "???").lower()
                if s == "processing": count_proc += 1
                elif s == "pending": count_queue += 1
                elif s == "complete": count_done += 1
                elif s == "failed": count_fail += 1
                
            # Reverse to show newest first (LIFO)
            cards.reverse()
            # Removed sorting by status to maintain strict timeline
            for c in cards[:15]: # Show top 15
                s = c.get("status", "???").lower()
                desc = c.get("description", "").split(".")[0] # Shorten
                
                # Status Styling
                style = C_TEXT
                if s == "complete": 
                    style = f"dim {C_LABEL}"
                elif s == "paused":
                    style = "dim #44475a"
                elif s in ["review", "in_progress"]:
                    style = C_WARN
                
                table.add_row(c.get("id"), s.upper(), desc, style=style)
    except: pass
    
    # Format Subtitle
    sub = f"[{C_ACCENT}]processing {count_proc}[/] | [{C_WARN}]queue {count_queue}[/] | [{C_PRIMARY}]complete {count_done}[/] | [{C_ERR}]failed {count_fail}[/]"
    
    return rich.panel.Panel(table, title="[bold #bd93f9]card_reader[/]", subtitle=sub, border_style=C_BORDER, box=rich.box.SQUARE, padding=(0,1))

class LogWatcher:
    def __init__(self, log_file, buffer_size=20):
        self.log_file = log_file
        self.buffer = deque(maxlen=buffer_size)
        self.last_pos = 0
        
        # Initial load
        try:
            if os.path.exists(self.log_file):
                with open(self.log_file, 'r') as f:
                    lines = f.readlines()
                    for line in lines[-buffer_size:]:
                        self.buffer.append(line.strip())
                    self.last_pos = f.tell()
            else:
                self.buffer.append("Log file not found: " + log_file)
        except Exception as e:
            self.buffer.append(f"Error loading log: {str(e)}")

    def scan(self):
        try:
            if os.path.exists(self.log_file):
                with open(self.log_file, 'r') as f:
                    f.seek(self.last_pos)
                    new_lines = f.readlines()
                    if new_lines:
                        self.last_pos = f.tell()
                        for line in new_lines:
                            self.buffer.append(line.strip())
        except Exception: pass

def main():
    root_layout = make_layout()
    watcher = LogWatcher(LOG_FILE)
    with rich.live.Live(root_layout, refresh_per_second=30, screen=True) as live:
        try:
            while True:
                watcher.scan()
                root_layout["header"].update(Header())
                root_layout["footer"].update(Footer())
                root_layout["cards"].update(cards_panel())
                root_layout["cpu_mem"].update(cpu_mem_panel())
                root_layout["net"].update(net_panel())
                root_layout["proc"].update(proc_panel(watcher))
                time.sleep(0.1)
        except KeyboardInterrupt: pass

if __name__ == "__main__":
    main()
