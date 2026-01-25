import json
import os
from datetime import datetime

# Mapping of "Deep Cut" 16-Bit Systems from repolist paper
DEEP_CUTS_MAP = {
    "WinUAE": {
        "url": "https://github.com/tonioni/WinUAE.git",
        "note": "WinUAE (Reference Amiga Emulator)",
        "category": "02_Computers"
    },
    "86Box": {
        "url": "https://github.com/86Box/86Box.git",
        "note": "86Box (Low-level PC Emulator)",
        "category": "02_Computers"
    },
    "FreeDOS": {
        "url": "https://github.com/FDOS/kernel.git",
        "note": "FreeDOS Kernel (MS-DOS Compatible)",
        "category": "02_Computers"
    },
    "AROS": {
        "url": "https://github.com/aros-development-team/AROS.git",
        "note": "AROS (AmigaOS 3.1 Re-implementation)",
        "category": "02_Computers"
    },
    "EmuTOS": {
        "url": "https://github.com/emutos/emutos.git",
        "note": "EmuTOS (Atari TOS Replacement)",
        "category": "02_Computers"
    },
    "Open_Watcom_v2": {
        "url": "https://github.com/open-watcom/open-watcom-v2.git",
        "note": "Open Watcom v2 (x86 C/C++ Compiler)",
        "category": "03_Toolchains"
    },
    "AQB": {
        "url": "https://github.com/gooofy/aqb.git",
        "note": "Amiga BASIC Compiler",
        "category": "03_Toolchains"
    },
    "VolksForth": {
        "url": "https://github.com/forth-ev/VolksForth.git",
        "note": "Forth-83 for 16-bit Systems",
        "category": "03_Toolchains"
    }
}

QUEUE_FILE = "runtime/card_queue.json"
ROOT_DIR = "oss_sovereignty/sys_16_16Bit_Revolution"

def mint_deep_cuts_16bit():
    print(">> MINTING 16-BIT DEEP CUT CARDS...")
    
    new_cards = []
    timestamp = datetime.now().isoformat()
    
    for sys_name, data in DEEP_CUTS_MAP.items():
        # Determine target path
        target_path = os.path.join(ROOT_DIR, data["category"], sys_name)
        if not os.path.exists(target_path):
            os.makedirs(target_path)

        # Create Card
        card = {
            "id": f"sov_16bit_deep_{sys_name.lower()}",
            "description": f"16-BIT DEEP: Assimilate {sys_name} ({data['note']})",
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
        
    print(f">> Minted {len(new_cards)} 16-BIT DEEP CUT cards.")

if __name__ == "__main__":
    mint_deep_cuts_16bit()
