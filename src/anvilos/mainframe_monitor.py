#!/usr/bin/env python3
import time
import sqlite3
import os
import sys
import termios
import tty
import select
import subprocess
from rich.layout import Layout
from rich.live import Live
from rich.panel import Panel
from rich.table import Table
from rich.text import Text
from rich.console import Console
from rich.align import Align
from rich import box
from datetime import datetime
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SYSTEM_DB = os.path.join(PROJECT_ROOT, "data", "cortex.db")
class MainframeMonitor:
    def __init__(self):
        self.console = Console()
        self.conn = sqlite3.connect(SYSTEM_DB, check_same_thread=False)
        self.cursor = self.conn.cursor()
        try:
            subprocess.run("pkill -f processor_daemon.py", shell=True)
            subprocess.run("pkill -f architect_daemon.py", shell=True)
        except Exception: pass
    def fetch_cards(self):
        try:
            self.cursor.execute("SELECT id, seq, op, pld, stat, ret, timestamp FROM card_stack ORDER BY timestamp DESC, seq DESC LIMIT 12")
            return self.cursor.fetchall()
        except Exception as e:
            return []
    def fetch_stats(self):
        try:
            self.cursor.execute("SELECT stat, COUNT(*) FROM card_stack GROUP BY stat")
            return dict(self.cursor.fetchall())
        except Exception as e:
            return {}
    def get_status_text(self, code):
        if code == 0: return "[bold blue]PENDING[/bold blue]"
        if code == 1: return "[yellow]PROCESSING[/yellow]"
        if code == 2: return "[green]PUNCHED[/green]"
        if code == 9: return "[red]JAM / ERR[/red]"
        return "?"
    def make_layout(self) -> Layout:
        layout = Layout()
        layout.split_column(
            Layout(name="header", size=3),
            Layout(name="body"),
            Layout(name="footer", size=3)
        )
        layout["body"].split_row(
            Layout(name="stack", ratio=2),
            Layout(name="telemetry", ratio=1)
        )
        return layout
    def generate_header(self):
        grid = Table.grid(expand=True)
        grid.add_column(justify="left", ratio=1)
        grid.add_column(justify="center", ratio=1)
        grid.add_column(justify="right", ratio=1)
        now = datetime.now().strftime("%H:%M:%S")
        grid.add_row(
            "[green]PROJECT ANVILOS_ALPHA[/green]",
            "[cyan]CORTEX MAINFRAME INTERFACE[/cyan]",
            f"[green]{now}[/green]"
        )
        return Panel(grid, style="green on black", box=box.SQUARE)
    def generate_stack_view(self):
        table = Table(expand=True, border_style="green", box=box.SIMPLE_HEAD)
        table.add_column("SEQ", justify="right", style="blue", header_style="blue", no_wrap=True)
        table.add_column("ID", style="magenta", header_style="magenta")
        table.add_column("OP CODE", style="yellow", header_style="yellow")
        table.add_column("PAYLOAD", style="cyan", header_style="cyan")
        table.add_column("STATUS", justify="center", style="green", header_style="green")
        cards = self.fetch_cards()
        for c in cards:
            c_id, seq, op, pld, stat, ret, ts = c
            pld_display = (pld[:30] + '..') if pld and len(pld) > 30 else (pld or "")
            table.add_row(
                f"{seq:04d}",
                c_id[:8],
                op,
                pld_display,
                self.get_status_text(stat)
            )
        return Panel(table, title="[INSTRUCTION STACK]", border_style="green")
    def generate_telemetry(self):
        stats = self.fetch_stats()
        pending = stats.get(0, 0)
        processing = stats.get(1, 0)
        completed = stats.get(2, 0)
        errors = stats.get(9, 0)
        total = pending + processing + completed + errors
        pct = (completed / total * 100) if total > 0 else 0
        try:
            res_p = subprocess.run("pgrep -f 'python3.*[p]rocessor_daemon.py'", shell=True, capture_output=True)
            res_a = subprocess.run("pgrep -f 'python3.*[a]rchitect_daemon.py'", shell=True, capture_output=True)
            p_running = (res_p.returncode == 0)
            a_running = (res_a.returncode == 0)
        except Exception:
            p_running = a_running = False
        if not p_running and not a_running:
            sys_status = "[bold dim red]OFFLINE[/bold dim red]"
        elif not p_running or not a_running:
            sys_status = "[bold yellow]DEGRADED[/bold yellow]"
        elif errors > 5:
            sys_status = "[bold red]ATTENTION REQ[/bold red]"
        elif processing > 0:
            sys_status = "[bold yellow]COMPUTING[/bold yellow]"
        else:
            sys_status = "[bold green]ONLINE[/bold green]"
        text = Text()
        text.append("\nSYSTEM STATUS: ", style="cyan")
        text.append_text(Text.from_markup(f"{sys_status}\n\n"))
        text.append(f"TOTAL CARDS : {total}\n", style="cyan")
        text.append(f"-------------------\n", style="blue")
        text.append(f"PENDING     : {pending}\n", style="blue")
        text.append(f"PROCESSING  : {processing}\n", style="yellow")
        text.append(f"PUNCHED     : {completed}\n", style="green")
        text.append(f"JAMMED      : {errors}\n", style="red")
        text.append(f"\nCOMPLETION  : {pct:.1f}%\n", style="magenta")
        return Panel(
            Align.center(text), 
            title="[SYSTEM TELEMETRY]", 
            border_style="green"
        )
    def generate_footer(self, status_msg=""):
        text = Text.from_markup(
            "[green]F1[/green] [green]START[/green] [blue]|[/blue] "
            "[magenta]F2[/magenta] [magenta]STOP[/magenta] [blue]|[/blue] "
            "[yellow]F3[/yellow] [yellow]RESET[/yellow] [blue]|[/blue] "
            "[cyan]F4[/cyan] [cyan]SORT[/cyan] [blue]|[/blue] "
            "[grey]F5[/grey] [grey]ARCHIVE[/grey] [blue]|[/blue] "
            "[red]ESC[/red] [red]QUIT[/red]"
        )
        if status_msg:
            text.append(" | ", style="blue")
            text.append(f"[{status_msg}]", style="bold red")
        return Panel(Align.center(text), style="green on black", box=box.SQUARE)
    def handle_input(self):
        if select.select([sys.stdin], [], [], 0)[0]:
            try:
                key = sys.stdin.read(1)
                if key == '\x1b':
                    if select.select([sys.stdin], [], [], 0.1)[0]:
                        key += sys.stdin.read(2)
                        if key == '\x1bOP': return "F1"
                        if key == '\x1bOQ': return "F2"
                        if key == '\x1bOR': return "F3"
                        if key == '\x1bOS': return "F4"
                        if key == '\x1b[1': # Extended sequences
                             key += sys.stdin.read(2)
                             if key == '\x1b[15~': return "F5"
                             if key == '\x1b[11~': return "F1"
                             if key == '\x1b[12~': return "F2"
                             if key == '\x1b[13~': return "F3"
                             if key == '\x1b[14~': return "F4"
                    return "ESC"
                if key == '1': return "F1"
                if key == '2': return "F2"
                if key == '3': return "F3"
                if key == '4': return "F4"
                if key == '5': return "F5"
                return key
            except Exception:
                return None
        return None
    def action_reload(self):
        try:
            subprocess.run("pkill -f processor_daemon.py", shell=True)
            subprocess.run("pkill -f architect_daemon.py", shell=True)
            time.sleep(0.5) # Give them a moment to die
            p_daemon = os.path.join(PROJECT_ROOT, "processor_daemon.py")
            a_daemon = os.path.join(PROJECT_ROOT, "architect_daemon.py")
            subprocess.Popen([sys.executable, p_daemon], cwd=PROJECT_ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            subprocess.Popen([sys.executable, a_daemon], cwd=PROJECT_ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception: pass
    def action_start(self):
        try:
            p_daemon = os.path.join(PROJECT_ROOT, "processor_daemon.py")
            a_daemon = os.path.join(PROJECT_ROOT, "architect_daemon.py")
            res_p = subprocess.run("pgrep -f 'python3.*[p]rocessor_daemon.py'", shell=True, capture_output=True)
            if res_p.returncode != 0:
                subprocess.Popen([sys.executable, p_daemon], cwd=PROJECT_ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            res_a = subprocess.run("pgrep -f 'python3.*[a]rchitect_daemon.py'", shell=True, capture_output=True)
            if res_a.returncode != 0:
                subprocess.Popen([sys.executable, a_daemon], cwd=PROJECT_ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception: pass
    def action_stop(self):
        try:
            subprocess.run("pkill -f processor_daemon.py", shell=True)
            subprocess.run("pkill -f architect_daemon.py", shell=True)
        except Exception: pass
    def action_sort(self):
        try:
            self.cursor.execute("SELECT id FROM card_stack WHERE stat=0 ORDER BY timestamp ASC")
            cards = self.cursor.fetchall()
            for idx, (c_id,) in enumerate(cards):
                self.cursor.execute("UPDATE card_stack SET seq=? WHERE id=?", (idx, c_id))
            self.conn.commit()
        except Exception: pass
    def action_archive(self):
        import json
        try:
            self.cursor.execute("SELECT * FROM card_stack")
            cols = [description[0] for description in self.cursor.description]
            cards = [dict(zip(cols, row)) for row in self.cursor.fetchall()]
            if not cards: return
            archive_path = os.path.join(PROJECT_ROOT, "data", "card_archive.json")
            existing_data = []
            if os.path.exists(archive_path):
                try:
                    with open(archive_path, 'r') as f:
                        existing_data = json.load(f)
                except Exception: pass
            existing_data.extend(cards)
            with open(archive_path, 'w') as f:
                json.dump(existing_data, f, indent=2)
            self.cursor.execute("DELETE FROM card_stack")
            self.cursor.execute("DELETE FROM sys_goals")
            self.conn.commit()
        except Exception: pass
    def run(self):
        old_settings = termios.tcgetattr(sys.stdin)
        status_msg = ""
        status_timer = 0
        try:
            tty.setcbreak(sys.stdin.fileno())
            layout = self.make_layout()
            layout["header"].update(self.generate_header())
            layout["body"]["stack"].update(self.generate_stack_view())
            layout["body"]["telemetry"].update(self.generate_telemetry())
            layout["footer"].update(self.generate_footer())
            with Live(layout, refresh_per_second=4, screen=False) as live:
                while True:
                    layout["header"].update(self.generate_header())
                    layout["body"]["stack"].update(self.generate_stack_view())
                    layout["body"]["telemetry"].update(self.generate_telemetry())
                    layout["footer"].update(self.generate_footer(status_msg))
                    if status_msg:
                        status_timer += 1
                        if status_timer > 10:
                            status_msg = ""
                            status_timer = 0
                    key = self.handle_input()
                    if key:
                         if key not in ["ESC", "F1", "F2", "F3", "F4", "F5"]:
                            status_msg = f"KEY: {repr(key)}"
                            status_timer = 0
                    if key == "ESC":
                        break
                    elif key == "F1":
                        self.action_start()
                        status_msg = "STARTING..."
                        status_timer = 0
                    elif key == "F2":
                        self.action_stop()
                        status_msg = "STOPPING..."
                        status_timer = 0
                    elif key == "F3":
                        self.action_reload()
                        status_msg = "RESETTING..."
                        status_timer = 0
                    elif key == "F4":
                        self.action_sort()
                        status_msg = "SORTING..."
                        status_timer = 0
                    elif key == "F5":
                        self.action_archive()
                        status_msg = "ARCHIVED ALL"
                        status_timer = 0
                    time.sleep(0.1)
        except KeyboardInterrupt:
            pass
        finally:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
            print("\nSYSTEM HALTED.")
if __name__ == "__main__":
    monitor = MainframeMonitor()
    monitor.run()
