import os
import time
import sqlite3
import json
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

CORTEX_DB_PATH = "/var/lib/anvilos/db/cortex.db"

class SpyHandler(FileSystemEventHandler):
    def __init__(self, agent_id="spy"):
        self.agent_id = agent_id

    def log_event(self, event_type, src_path, is_directory):
        try:
            with sqlite3.connect(CORTEX_DB_PATH) as conn:
                conn.execute("""
                    INSERT INTO live_stream (agent_id, event_type, details)
                    VALUES (?, ?, ?)
                """, (self.agent_id, "AUDIT_FILE", json.dumps({
                    "type": event_type,
                    "path": src_path,
                    "is_dir": is_directory
                })))
                conn.commit()
        except Exception as e:
            print(f"[Spy Error] {e}")

    def on_modified(self, event):
        self.log_event("MODIFIED", event.src_path, event.is_directory)

    def on_created(self, event):
        self.log_event("CREATED", event.src_path, event.is_directory)

    def on_deleted(self, event):
        self.log_event("DELETED", event.src_path, event.is_directory)

def start_spy(watch_path):
    event_handler = SpyHandler()
    observer = Observer()
    observer.schedule(event_handler, watch_path, recursive=True)
    observer.start()
    print(f"[SPY] Monitoring {watch_path}")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        observer.stop()
    observer.join()

if __name__ == "__main__":
    # Monitor the project root and the build directory
    import sys
    path = sys.argv[1] if len(sys.argv) > 1 else "/mnt/anvil_temp/github/anvilos_alpha"
    start_spy(path)
