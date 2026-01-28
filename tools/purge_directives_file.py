#!/usr/bin/env python3
import sys
import os
import re

# Add project root to path
sys.path.append(os.getcwd())

try:
    from forge_directives import PHASE2_DIRECTIVES
except ImportError:
    print("Error: Could not import PHASE2_DIRECTIVES")
    sys.exit(1)

print(f"Loaded {len(PHASE2_DIRECTIVES)} directives.")

# Mandate: No non-English locales/docs
# .po, .mo, man/xx/
patterns = [
    r"\.po$",
    r"\.mo$",
    r"/man/[a-z]{2}/",
    r"/man/[a-z]{2}_[A-Z]{2}/",
    r"/po/",
    r"/po-man/"
]
compiled = [re.compile(p) for p in patterns]

cleaned = []
violations = 0

for card in PHASE2_DIRECTIVES:
    is_bad = False
    if 'pld' in card and 'path' in card['pld']:
        path = card['pld']['path']
        for p in compiled:
            if p.search(path):
                is_bad = True
                violations += 1
                break
    
    if not is_bad:
        cleaned.append(card)

print(f"Found {violations} violations.")
print(f"Remaining: {len(cleaned)}")

if violations > 0:
    print("Writing sanitized forge_directives.py...")
    with open('forge_directives.py', 'w') as f:
        f.write("PHASE2_DIRECTIVES = [\n")
        for card in cleaned:
            f.write("    " + repr(card) + ",\n")
        f.write("]\n")
    print("Purge successful.")
else:
    print("No violations found.")
