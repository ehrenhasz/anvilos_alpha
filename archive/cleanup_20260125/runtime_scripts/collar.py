import sys
import os
import subprocess
import json
import time
from typing import Any, Dict, Optional, Union
from synapse import Synapse
class TheCollar:
    """
    The Collar: An oversight mechanism that wraps operations to ensure
    all actions, successes, and failures are logged to the Cortex via Synapse.
    """
    def __init__(self, agent_id: str):
        self.synapse = Synapse(agent_id)
        self.agent_id = agent_id
    def log(self, event_type: str, context: str, success: bool, details: Dict[str, Any]):
        """Directly log an event."""
        self.synapse.log_experience(event_type, context, success, details)
    def guard(self, context: str, operation_name: str, func, *args, **kwargs) -> Any:
        """
        Wraps a Python function execution, logging the outcome.
        """
        start_time = time.time()
        try:
            result = func(*args, **kwargs)
            duration = time.time() - start_time
            self.synapse.log_experience(
                "OP_SUCCESS",
                context,
                True,
                {
                    "op": operation_name,
                    "duration": f"{duration:.4f}s",
                    "result_summary": str(result)[:100]
                }
            )
            return result
        except Exception as e:
            duration = time.time() - start_time
            self.synapse.log_experience(
                "OP_FAILURE",
                context,
                False,
                {
                    "op": operation_name,
                    "duration": f"{duration:.4f}s",
                    "error": str(e),
                    "trace": str(sys.exc_info())
                }
            )
            raise e
    def sh(self, command: str, context: str = "SHELL") -> subprocess.CompletedProcess:
        """
        Executes a shell command and enforces logging of the result.
        Returns the CompletedProcess object.
        """
        start_time = time.time()
        try:
            result = subprocess.run(command, shell=True, capture_output=True, text=True)
            duration = time.time() - start_time
            success = (result.returncode == 0)
            status = "SHELL_EXEC"
            details = {
                "command": command,
                "return_code": result.returncode,
                "stdout": result.stdout.strip()[:1000],  # Capture reasonable amount
                "stderr": result.stderr.strip()[:1000],
                "duration": f"{duration:.4f}s"
            }
            self.synapse.log_experience(status, context, success, details)
            return result
        except Exception as e:
            self.synapse.log_experience("SHELL_CRASH", context, False, {"command": command, "error": str(e)})
            raise e
def attach_collar(agent_id: str) -> TheCollar:
    return TheCollar(agent_id)
