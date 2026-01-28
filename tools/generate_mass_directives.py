#!/usr/bin/env python3
import os
import sys
import pprint

# Import existing directives to preserve the build logic
sys.path.append(os.path.join(os.path.dirname(__file__), ".."))
try:
    from forge_directives import PHASE2_DIRECTIVES as CORE_LOGIC
except ImportError:
    CORE_LOGIC = []

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SOURCES_ROOT = os.path.join(PROJECT_ROOT, "oss_sovereignty", "sources")
OUTPUT_FILE = os.path.join(PROJECT_ROOT, "forge_directives.py")

def generate_mass_manifest():
    print(f"[*] Scanning {SOURCES_ROOT}...")
    mass_cards = []
    
    # 1. PRE-FLIGHT (Keep original seq 1-99)
    pre_flight = [c for c in CORE_LOGIC if c.get('seq', 0) < 100]
    
    # 2. MASS VERIFICATION (Seq 100000+)
    # We use high sequence numbers to avoid collision, or we insert them between pre-flight and build.
    # Let's insert them as "Phase 0.5".
    
    seq_counter = 1000
    for root, dirs, files in os.walk(SOURCES_ROOT):
        for file in files:
            abs_path = os.path.join(root, file)
            rel_path = os.path.relpath(abs_path, PROJECT_ROOT)
            
            # The Card
            card = {
                "seq": seq_counter,
                "op": "verify_file_integrity",
                "pld": {
                    "path": rel_path,
                    "desc": f"Verify presence of {os.path.basename(file)}"
                }
            }
            mass_cards.append(card)
            seq_counter += 1
            
    print(f"[*] Generated {len(mass_cards)} verification cards.")
    
    # 3. BUILD LOGIC (Shift sequence to run AFTER verification)
    # Original build logic starts at 100. We need to shift it or just append it.
    # If we append it, the "verify" happens first? No, we need to sort by SEQ.
    # Let's verify FIRST.
    
    # Re-index build logic to start after the last verification card
    build_logic = [c for c in CORE_LOGIC if c.get('seq', 0) >= 100]
    
    build_start_seq = seq_counter + 1000
    for card in build_logic:
        # Keep relative order but shift base
        # Logic: NewSeq = BuildStart + (OldSeq - 100)
        # This preserves gaps between 100, 200, 300 etc.
        offset = card['seq']
        card['seq'] = build_start_seq + offset
    
    # Combine
    full_manifest = pre_flight + mass_cards + build_logic
    
    # Sort just in case
    full_manifest.sort(key=lambda x: x['seq'])
    
    print(f"[*] Total Manifest Size: {len(full_manifest)} directives.")
    
    # WRITE
    with open(OUTPUT_FILE, "w") as f:
        f.write("# Forge Directives - MASSIVE GRANULARITY MODE\n\n")
        f.write("PHASE2_DIRECTIVES = [\n")
        for card in full_manifest:
            f.write("    " + pprint.pformat(card, width=120) + ",\n")
        f.write("]\n")
        
    print(f"[+] Written to {OUTPUT_FILE}")

if __name__ == "__main__":
    generate_mass_manifest()
