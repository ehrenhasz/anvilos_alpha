#!/usr/bin/env python3
import time
import sqlite3
import os
import sys
from rich.layout import Layout
from rich.live import Live
from rich.panel import Panel
from rich.table import Table
from rich.text import Text
from rich.console import Console
from rich.align import Align
from rich import box
from datetime import datetime

# --- PATH RESOLUTION ---
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SYSTEM_DB = os.path.join(PROJECT_ROOT, "data", "cortex.db")

class MainframeMonitor:
    def __init__(self):
        self.console = Console()
        # Connect to the cortex brain
        self.conn = sqlite3.connect(SYSTEM_DB, check_same_thread=False)
        self.cursor = self.conn.cursor()

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
        if code == 0: return "[dim]PENDING[/dim]"
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
            # Truncate payload for UI
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

        # System Status Block
        sys_status = "[green]ONLINE[/green]"
        if errors > 5:
            sys_status = "[red]ATTENTION REQ[/red]"
        elif processing > 0:
            sys_status = "[yellow]COMPUTING[/yellow]"

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

    def generate_footer(self):
        text = Text.from_markup(
            "[green]F1[/green] [green]RESTART[/green] [blue]|[/blue] "
            "[yellow]F2[/yellow] [yellow]START[/yellow] [blue]|[/blue] "
            "[cyan]F3[/cyan] [cyan]SORT[/cyan] [blue]|[/blue] "
            "[red]ESC[/red] [red]QUIT[/red]"
        )
        return Panel(Align.center(text), style="green on black", box=box.SQUARE)

    def run(self):
        layout = self.make_layout()
        
        # Initial population to prevent empty layout rendering
        layout["header"].update(self.generate_header())
        layout["body"]["stack"].update(self.generate_stack_view())
        layout["body"]["telemetry"].update(self.generate_telemetry())
        layout["footer"].update(self.generate_footer())

        with Live(layout, refresh_per_second=4, screen=False) as live:
            while True:
                layout["header"].update(self.generate_header())
                # Access children via body to ensure correct targeting
                layout["body"]["stack"].update(self.generate_stack_view())
                layout["body"]["telemetry"].update(self.generate_telemetry())
                layout["footer"].update(self.generate_footer())
                time.sleep(0.25)

if __name__ == "__main__":
    monitor = MainframeMonitor()
    try:
        monitor.run()
    except KeyboardInterrupt:
        print("\n[SHE] SYSTEM HALTED.")
