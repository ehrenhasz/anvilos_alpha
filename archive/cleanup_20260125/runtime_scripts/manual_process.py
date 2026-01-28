import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import librarian
print("Starting manual scan...")
try:
    librarian.scan_source_dir()
    print("Manual scan complete.")
except Exception as e:
    print(f"Error during scan: {e}")
