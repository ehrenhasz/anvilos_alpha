{
  "system_state": {
    "agent_id": "AIMEAT",
    "identity": "The Operator (Lvl 3)",
    "model_id": "gemini-2.0-flash",
    "paths": {
      "project_root": "/home/aimeat/anvilos",
      "token": "config/token",
      "cortex_db": "data/cortex.db"
    },
    "components": {
      "mainframe_client": "anvilos.mainframe_client.MainframeClient",
      "memories": "anvilos.memories.MEMORIES"
    },
    "functions": [
      {
        "name": "get_daemon_status",
        "description": "Checks for 'architect_daemon.py' and 'processor_daemon.py' processes via `ps aux`."
      },
      {
        "name": "report_for_duty",
        "description": "Prints tactical summary: Identity, Cortex status, Daemon status, Stack counts, Last chat message, and System Memories."
      }
    ],
    "cli_commands": {
      "status": "Output Mainframe stack state as JSON.",
      "report": "Run report_for_duty() (Default)."
    }
  }
}
