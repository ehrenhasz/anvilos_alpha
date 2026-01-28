#!/usr/bin/env python3
import time
import os
import sys
import random
import glob
from rich.console import Console
from rich.live import Live
from rich.table import Table
from rich.panel import Panel
from rich.text import Text
from rich import box
PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
ARTIFACT_DIRS = [
    os.path.join(PROJECT_ROOT, "src", "native"),
    os.path.join(PROJECT_ROOT, "build_artifacts"),
    os.path.join(PROJECT_ROOT, "tests", "anvil")
]
EXTENSIONS = ["*.anv", "*.json", "*.mpy", "*.c", "*.rs", "*.h"]
COLORS = [
    "bright_magenta", "bright_cyan", "bright_green", "bright_yellow", 
    "orange1", "deep_pink1", "spring_green1", "turquoise2", "violet",
    "bright_red", "hot_pink", "gold1", "cyan1", "medium_purple1"
]
import termios
import tty
import select
class MatrixStream:
    def __init__(self):
        self.console = Console()
        self.files_seen = set()
        self.content_buffer = []  # List of (filename, line_text, color)
        self.scroll_index = 0
        self.max_display_lines = 25 # Standard terminal height
        self.last_scan = 0
        self.paused = False
        self.scan_and_ingest()
    def get_random_color(self):
        return random.choice(COLORS)
    def scan_and_ingest(self):
        found_files = []
        for d in ARTIFACT_DIRS:
            for ext in EXTENSIONS:
                found_files.extend(glob.glob(os.path.join(d, "**", ext), recursive=True))
        found_files.sort(key=os.path.getmtime, reverse=True)
        for f in found_files:
            if f not in self.files_seen:
                self.files_seen.add(f)
                self.ingest_file(f)
        if not self.content_buffer and found_files:
            for f in found_files:
                self.ingest_file(f)
    def ingest_file(self, filepath):
        try:
            fname = os.path.basename(filepath)
            header_color = "bold italic white on deep_pink1"
            self.content_buffer.append((fname, f"--- LOADING: {fname} ---", header_color))
            with open(filepath, 'r', errors='replace') as f:
                lines = f.readlines()[:100]
            file_color = self.get_random_color()
            for line in lines:
                clean_line = line.rstrip()
                if clean_line:
                    color = file_color if random.random() > 0.3 else self.get_random_color()
                    self.content_buffer.append((fname, clean_line, color))
            self.content_buffer.append((fname, " ", "black")) # Spacer
        except Exception:
            pass
    def get_view(self):
        if not self.content_buffer:
            return Panel(
                Text("SCANNING FOR ANVIL ARTIFACTS...", style="blink bold magenta"),
                border_style="bright_cyan"
            )
        table = Table(show_header=False, box=None, expand=True, padding=(0, 0))
        table.add_column("Source", width=15, style="dim turquoise2", justify="right")
        table.add_column("Stream", ratio=1)
        total = len(self.content_buffer)
        for i in range(self.max_display_lines):
            idx = (self.scroll_index + i) % total
            fname, text, color = self.content_buffer[idx]
            table.add_row(fname[:14], Text(text, style=color))
        if not self.paused:
            self.scroll_index = (self.scroll_index + 1) % total
        title = "[bold bright_magenta]PRIDE IN THE MACHINE: THE CODE STREAM[/bold bright_magenta]"
        if self.paused:
            title += " [bold white on red] PAUSED [/bold white on red]"
        return Panel(
            table, 
            title=title, 
            border_style="bright_green", 
            box=box.DOUBLE
        )
    def check_input(self):
        if select.select([sys.stdin], [], [], 0)[0]:
            key = sys.stdin.read(1)
            if key == ' ':
                self.paused = not self.paused
            return key
        return None
    def run(self):
        import shutil
        os.system('cls' if os.name == 'nt' else 'clear')
        self.console.print(Panel("[bold bright_magenta]PRIDE IN THE MACHINE: THE CODE STREAM[/bold bright_magenta]", border_style="bright_green", box=box.DOUBLE))
        old_settings = termios.tcgetattr(sys.stdin)
        try:
            tty.setcbreak(sys.stdin.fileno())
            self.console.show_cursor(False)
            while True:
                key = self.check_input()
                if key == 'q':
                    break
                height = shutil.get_terminal_size().lines
                effective_height = max(10, height - 5)
                line_delay = 3.0 / effective_height
                if not self.paused:
                    if time.time() - self.last_scan > 5.0:
                        self.scan_and_ingest()
                        self.last_scan = time.time()
                    total = len(self.content_buffer)
                    if total > 0:
                        idx = self.scroll_index % total
                        fname, text, color = self.content_buffer[idx]
                        prefix = Text(f"{fname[:14]:>14} | ", style="dim turquoise2")
                        content = Text(text, style=color)
                        self.console.print(prefix + content)
                        self.scroll_index += 1
                    time.sleep(line_delay)
                else:
                    time.sleep(0.1)
        finally:
            self.console.show_cursor(True)
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
if __name__ == "__main__":
    try:
        MatrixStream().run()
    except KeyboardInterrupt:
        pass
