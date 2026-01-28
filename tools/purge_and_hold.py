#!/usr/bin/env python3
import sys
import os
import pprint

# Add project root to path
sys.path.append(os.getcwd())

try:
    from forge_directives import PHASE2_DIRECTIVES
except ImportError:
    print("Error: Could not import PHASE2_DIRECTIVES")
    sys.exit(1)

print(f"Loaded {len(PHASE2_DIRECTIVES)} directives.")

# Filter: Keep only 'verify_file_integrity' and sequence < 100 (setup/toolchain checks)
# Mandate: "remove everything else" implies stripping the build/compile logic
retained = []
for card in PHASE2_DIRECTIVES:
    if card.get('seq', 999999) < 100:
        retained.append(card)
    elif card.get('op') == 'verify_file_integrity':
        retained.append(card)

print(f"Purging build logic. Retained: {len(retained)} verification/setup directives.")

with open('forge_directives.py', 'w') as f:
    f.write("PHASE2_DIRECTIVES = [\n")
    for card in retained:
        # Using repr to ensure valid python dict syntax
        f.write("    " + pprint.pformat(card, width=120, sort_dicts=False) + ",\n")
    f.write("]\n")

print("Purge complete. System is now in HOLD state (Integrity Check Only).")
