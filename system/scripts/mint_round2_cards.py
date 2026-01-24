import json
import os
from datetime import datetime

# Mapping of "Round 2" systems
ROUND2_MAP = {
    # --- HISTORICAL OS ---
    "PLAN_9": {
        "url": "https://github.com/rsc/plan9.git", 
        "note": "Plan 9 from Bell Labs (Legacy Bootable)"
    },
    "MINIX_3": {
        "url": "https://github.com/Stichting-MINIX-Research-Foundation/minix.git",
        "note": "Minix 3 Microkernel OS"
    },
    "HAIKU": {
        "url": "https://github.com/haiku/haiku.git",
        "note": "Haiku (BeOS Re-implementation)"
    },
    
    # --- NETWORK STACKS ---
    "DECNET_LINUX": {
        "url": "https://github.com/JohnForecast/LinuxDECnet.git",
        "note": "DECnet Module for Linux"
    },
    "IPX_LINUX": {
        "url": "https://github.com/pasis/ipx.git",
        "note": "IPX/SPX Module for Linux"
    },
    
    # --- ACCESSORS ---
    "GHOSTSCRIPT": {
        "url": "https://github.com/ArtifexSoftware/ghostpdl.git",
        "note": "PostScript & PDF Interpreter (The Printer)"
    },
    
    # --- LANGUAGES ---
    "SMALLTALK": {
        "url": "https://github.com/squeak-smalltalk/squeak-app.git",
        "note": "Squeak Smalltalk Environment"
    }
}

QUEUE_FILE = "runtime/card_queue.json"
ROOT_DIR = "oss_sovereignty"

def mint_round2():
    print(">> MINTING ROUND 2 CARDS (HISTORY & ALT)...")
    
    new_cards = []
    timestamp = datetime.now().isoformat()
    
    for sys_name, data in ROUND2_MAP.items():
        # Determine target path
        target_path = None
        
        # Heuristic placement
        if "PLAN_9" in sys_name: target_path = "oss_sovereignty/sys_13_Platforms/04_Unix_Commercial/PLAN_9"
        elif "MINIX" in sys_name: target_path = "oss_sovereignty/sys_13_Platforms/04_Unix_Commercial/MINIX_3"
        elif "HAIKU" in sys_name: target_path = "oss_sovereignty/sys_13_Platforms/06_Microcomputer/HAIKU"
        elif "DECNET" in sys_name: target_path = "oss_sovereignty/sys_14_Accessors/03_Legacy_Net/DECNET_LINUX"
        elif "IPX" in sys_name: target_path = "oss_sovereignty/sys_14_Accessors/03_Legacy_Net/IPX_LINUX"
        elif "GHOSTSCRIPT" in sys_name: target_path = "oss_sovereignty/sys_14_Accessors/01_Terminals/GHOSTSCRIPT"
        elif "SMALLTALK" in sys_name: target_path = "oss_sovereignty/sys_12_Languages/04_Managed/SMALLTALK"
        
        if not target_path:
             target_path = f"oss_sovereignty/sys_13_Platforms/99_Round2/{sys_name}"

        if not os.path.exists(target_path):
            os.makedirs(target_path, exist_ok=True)
            
        # Create Card
        card = {
            "id": f"sov_r2_{sys_name.lower()}",
            "description": f"ROUND 2: Assimilate {sys_name} ({data['note']})",
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
        
    print(f">> Minted {len(new_cards)} ROUND 2 cards.")

if __name__ == "__main__":
    mint_round2()
