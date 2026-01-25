import os
import json
from datetime import datetime

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

class MicroLogger:
    def __init__(self, agent_id):
        self.log_path = os.path.join(PROJECT_ROOT, "ext", f"{agent_id}.log")
        os.makedirs(os.path.dirname(self.log_path), exist_ok=True)

    def log(self, event_id: int, data: str):
        payload = {"@ID": event_id, "data": data, "ts": datetime.now().isoformat()}
        try:
            with open(self.log_path, "a") as f:
                f.write(json.dumps(payload) + "\n")
        except: pass
