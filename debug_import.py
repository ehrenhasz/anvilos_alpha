import sys
import os
PROJECT_ROOT = os.getcwd()
sys.path.append(os.path.join(PROJECT_ROOT, "vendor"))
print(f"sys.path: {sys.path}")
try:
    import typing_inspection
    print("Import typing_inspection SUCCESS")
except ImportError as e:
    print(f"Import typing_inspection FAILED: {e}")
try:
    from pydantic import errors
    print("Import pydantic.errors SUCCESS")
except ImportError as e:
    print(f"Import pydantic.errors FAILED: {e}")
