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

unique_directives = []
seen_paths = set()
duplicates_count = 0

for card in PHASE2_DIRECTIVES:
    # Always keep non-file-integrity checks (like setup ops)
    if card.get('op') != 'verify_file_integrity':
        unique_directives.append(card)
        continue

    # For integrity checks, deduplicate based on path
    path = card.get('pld', {}).get('path')
    if path:
        if path in seen_paths:
            duplicates_count += 1
        else:
            seen_paths.add(path)
            unique_directives.append(card)
    else:
        # Keep malformed cards to be safe/visible? Or drop? 
        # Keeping them for now to avoid accidental data loss.
        unique_directives.append(card)

print(f"Removed {duplicates_count} duplicates.")
print(f"Retained {len(unique_directives)} unique directives.")

with open('forge_directives.py', 'w') as f:
    f.write("PHASE2_DIRECTIVES = [\n")
    for card in unique_directives:
        # Use sort_dicts=False to maintain readability order
        f.write("    " + pprint.pformat(card, width=120, sort_dicts=False) + ",\n")
    f.write("]\n")

print("Deduplication complete.")
