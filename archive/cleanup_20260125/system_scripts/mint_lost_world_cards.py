import json
import os
from datetime import datetime

# Mapping of "Lost" systems to discovered repositories
LOST_WORLD_MAP = {
    # --- MAINFRAMES & PLATFORMS ---
    "z_OS": {
        "url": "https://github.com/moshix/MVS38j.SYS1.MACLIB.git", 
        "note": "Proxy: MVS 3.8j (Last Public Domain Release)"
    },
    "z_VM": {
        "url": "https://github.com/s390guy/vm370.git", # Mirror of VM/370 R6
        "note": "Proxy: VM/370 Release 6"
    },
    "TOPS_10": {
        "url": "https://github.com/PDP-10/tops10.git",
        "note": "DEC TOPS-10"
    },
    "VMS": {
        "url": "https://github.com/vms-ports/crtl.git", # Partial/Proxy (CRTL) or simh scripts
        "note": "Partial Proxy: OpenVMS CRTL / Simh Scripts" 
    },
    "MULTICS": {
        "url": "https://github.com/dqv/multics.git", # Multics MR 12.7
        "note": "Multics MR 12.7"
    },
    "CP_M": {
        "url": "https://github.com/dwhinham/cpm-source.git",
        "note": "CP/M 2.2"
    },
    "AGC": {
        "url": "https://github.com/chrislgarry/Apollo-11.git",
        "note": "Apollo Guidance Computer (Comanche055)"
    },
    
    # --- LANGUAGES ---
    "PL_I": {
        "url": "https://github.com/iron-spring/pli-2000.git",
        "note": "Iron Spring PL/I Compiler"
    },
    "SCHEME": {
        "url": "https://github.com/cisco/ChezScheme.git",
        "note": "Chez Scheme"
    },
    "RACKET": {
        "url": "https://github.com/racket/racket.git",
        "note": "Racket"
    },
    "MATLAB": {
        "url": "https://github.com/gnu-octave/octave.git", # Best Open Proxy
        "note": "Proxy: GNU Octave"
    },
    "SAS": {
        "url": "https://github.com/wesm/pandas.git", # Conceptual Proxy only
        "note": "Proxy: Pandas (Conceptual) - NO DIRECT SOURCE"
    },
    "BEFUNGE": {
        "url": "https://github.com/catseye/Befunge-93.git",
        "note": "Befunge-93 Reference"
    },
    
    # --- NETWORK ---
    "SNA": {
        "url": "https://github.com/moshix/MVS38j.SYS1.MACLIB.git", # SNA macros are inside MVS
        "note": "SNA Macros within MVS 3.8j"
    }
}

QUEUE_FILE = "runtime/card_queue.json"
ROOT_DIR = "oss_sovereignty"

def mint_lost_cards():
    print(">> MINTING CARDS FOR THE LOST WORLD...")
    
    new_cards = []
    timestamp = datetime.now().isoformat()
    
    for sys_name, data in LOST_WORLD_MAP.items():
        # Find the path in the directory structure (audit required or simple search)
        # We'll search for the folder name matching sys_name
        
        target_path = None
        for root, dirs, files in os.walk(ROOT_DIR):
            if sys_name in dirs:
                target_path = os.path.join(root, sys_name)
                break
            
            # Handle naming mismatches (e.g., CP_M vs DOS/Microcomputer)
            if sys_name == "CP_M" and "DOS" in dirs: # Put CP/M near DOS? 
                # Create if not exists
                target_path = "oss_sovereignty/sys_13_Platforms/06_Microcomputer/CP_M"
            
            if sys_name == "MULTICS" and "UNIX_Commercial" in root: # Loose fit
                 target_path = "oss_sovereignty/sys_13_Platforms/04_Unix_Commercial/MULTICS"
                 
            if sys_name == "AGC" and "Embedded" in root:
                 target_path = "oss_sovereignty/sys_13_Platforms/07_Embedded/AGC"

        if not target_path:
            # Fallback creation
            target_path = f"oss_sovereignty/sys_13_Platforms/99_Recovered/{sys_name}"
        
        if not os.path.exists(target_path):
            os.makedirs(target_path, exist_ok=True)
            
        # Create Card
        card = {
            "id": f"sov_lost_{sys_name.lower()}",
            "description": f"LOST WORLD: Assimilate {sys_name} ({data['note']})",
            "status": "pending",
            "command": f"python3 system/scripts/assimilate.py {target_path} {data['url']}",
            "created_at": timestamp,
            "metadata": {
                "system": sys_name,
                "proxy": data['note']
            }
        }
        new_cards.append(card)

    # Append to Queue
    if os.path.exists(QUEUE_FILE):
        with open(QUEUE_FILE, "r") as f:
            queue = json.load(f)
    else:
        queue = []
        
    queue.extend(new_cards)
    
    with open(QUEUE_FILE, "w") as f:
        json.dump(queue, f, indent=2)
        
    print(f">> Minted {len(new_cards)} LOST WORLD cards.")

if __name__ == "__main__":
    mint_lost_cards()
