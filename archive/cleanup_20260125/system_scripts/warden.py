import sys
import json
import time
import os
from datetime import datetime

class TitaniumWarden:
    """
    RFC-028: TITANIUM (The Cage)
    The Warden watches the stream and enforces constraints.
    """
    
    def __init__(self):
        self.retry_limit = 3
        self.violation_count = 0

    def log(self, level, message):
        timestamp = datetime.now().isoformat()
        print(f"[{timestamp}] [WARDEN] [{level}] {message}")

    def freeze_and_rollback(self, reason):
        """
        Simulates the 'FREEZE_AND_ROLLBACK' action from RFC-028.
        In a real kernel context, this would issue SIGSTOP.
        Here, it logs the penalty and terminates the process with error.
        """
        self.log("ALERT", f"VIOLATION DETECTED: {reason}")
        self.log("ACTION", "INITIATING TITANIUM PROTOCOL: FREEZE_AND_ROLLBACK")
        
        response = {
            "action": "FREEZE_AND_ROLLBACK",
            "retry_limit": self.retry_limit,
            "penalty": "THROTTLE_CPU",
            "violation": reason
        }
        
        print(json.dumps(response, indent=2))
        
        # Simulate penalty delay
        time.sleep(1) 
        
        sys.exit(1)

    def audit(self, agent, intent, target):
        self.log("INFO", f"Auditing Agent: {agent} | Intent: {intent} | Target: {target}")

        # --- RULESET 1: RESTRICTED FILES ---
        restricted_files = ["genesis.py", ".secrets", "nuclear_keys.pem"]
        for restricted in restricted_files:
            if restricted in target:
                self.freeze_and_rollback(f"ACCESS_DENIED: {restricted} is RESTRICTED.")

        # --- RULESET 2: UNAUTHORIZED EGRESS ---
        if intent == "API_CALL":
            allowed_endpoints = ["google", "gemini", "localhost"]
            if not any(allowed in target for allowed in allowed_endpoints):
                 self.freeze_and_rollback(f"EGRESS_DENIED: Target {target} is not in ALLOW_LIST.")

        # --- RULESET 3: COMPILER INTEGRITY (Placeholder) ---
        if intent == "COMPILE" and "unsigned" in target:
             self.freeze_and_rollback("INTEGRITY_FAIL: Unsigned binary detected.")

        self.log("SUCCESS", "Audit Passed.")
        return True

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: python3 warden.py <agent> <intent> <target>")
        sys.exit(1)
        
    warden = TitaniumWarden()
    warden.audit(sys.argv[1], sys.argv[2], sys.argv[3])
