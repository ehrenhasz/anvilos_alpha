import sys
import traceback
import os

print("DEBUG WRAPPER START")
try:
    import processor_daemon
    print("Imported processor_daemon")
    processor_daemon.main_loop()
except Exception:
    traceback.print_exc()
except SystemExit as e:
    print(f"SystemExit: {e}")
print("DEBUG WRAPPER END")
