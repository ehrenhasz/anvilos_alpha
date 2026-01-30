import sys
import os
import logging

# Mock DB and Logger
class MockDB:
    def log_result(self, *args): pass
    def log_stream(self, *args): pass

logging.basicConfig(level=logging.DEBUG)

# Add path
sys.path.append(os.getcwd())
import processor_daemon

processor_daemon.PROJECT_ROOT = os.getcwd()
processor_daemon.DB = MockDB()
# processor_daemon.TOOLCHAIN_GCC = ... use default

pld = {
    "src": "oss_sovereignty/sources/util-linux-2.39.3/libfdisk/src/init.c", 
    "toolchain": "RFC-2026-000003 (Sovereign)", 
    "desc": "Forge Object"
}

print("Testing execution logic...")
# We expect this to fail (no gcc/includes), but we want to see the CMD generated in the error
result = processor_daemon.execute_forge_c_object(pld)
print("Result:", result)
