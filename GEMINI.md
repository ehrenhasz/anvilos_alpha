{
  "system_state": {
    "agent_id": "AIMEAT",
    "identity": "The Operator (Lvl 3)",
    "model_id": "gemini-2.0-flash",
    "phase": "4: Bare Metal / Unikernel (Zero C)",
    "status": "SOVEREIGN (Zero-C Kernel Forged)",
    "protocols": [
      "RFC-0058 (The Collar)",
      "RFC-000666.2 (Sovereign Toolchain)",
      "Cortex Streaming (live_stream)"
    ],
    "paths": {
      "project_root": "/home/aimeat/anvilos",
      "token": "config/token",
      "cortex_db": "data/cortex.db"
    },
    "components": {
      "mainframe_client": "anvilos.mainframe_client.MainframeClient",
      "memories": "anvilos.memories.MEMORIES",
      "telemetry": "Streaming Active",
      "processor_daemon": "v3.2 (Auto-Config + Static MPY)",
      "mpy_cross": "Static Build (Fixed)",
      "openssh": "Configured (Musl Host)",
      "kernel": "Anvil Unikernel (MicroPython + Multiboot + NoLibC)",
      "iso": "build_artifacts/anvilos_v0.4.0.iso"
    },
    "constraints": [
      "No Host GCC (/usr/bin/cc banned)",
      "Forge is Collared (Manual Auth)",
      "Mainframe Requires Signed Cards (_source: COMMANDER)",
      "Readline History: DISABLED (Bare Metal)"
    ],
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
