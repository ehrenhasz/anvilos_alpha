import json
import os
from datetime import datetime

# Mapping of 16-Bit Systems
BIT16_MAP = {
    # --- CONSOLES ---
    "SNES": {
        "url": "https://github.com/snes9xgit/snes9x.git",
        "note": "Snes9x (Portable SNES Emulator)"
    },
    "Sega_Genesis": {
        "url": "https://github.com/ekeeke/Genesis-Plus-GX.git",
        "note": "Genesis Plus GX (Accurate Genesis/MegaDrive)"
    },
    "TurboGrafx_16": {
        "url": "https://github.com/libretro-mirrors/mednafen-git.git",
        "note": "Mednafen (Multi-system, covers PCE/TG16)"
    },
    "Neo_Geo": {
        "url": "https://github.com/finalburnneo/FBNeo.git",
        "note": "FinalBurn Neo (Arcade/NeoGeo)"
    },
    
    # --- COMPUTERS ---
    "Amiga_Hardware": {
        "url": "https://github.com/FrodeSolheim/fs-uae.git",
        "note": "FS-UAE (Amiga Emulator)"
    },
    "Atari_ST": {
        "url": "https://github.com/hatari/hatari.git",
        "note": "Hatari (Atari ST/STE/TT/Falcon)"
    },
    "Apple_IIGS": {
        "url": "https://github.com/digarok/gsplus.git",
        "note": "GSplus (Modern Apple IIGS Emulator)"
    },
    "Sharp_X68000": {
        "url": "https://github.com/libretro/px68k-libretro.git",
        "note": "PX68k (Portable X68000 Emulator)"
    },
    "PC_98": {
        "url": "https://github.com/AZO234/NP2kai.git",
        "note": "Neko Project II Kai (PC-9801 Emulator)"
    },
    
    # --- TOOLCHAINS ---
    "M68k_Tools": {
        "url": "https://github.com/dbuchwald/vasm.git",
        "note": "vasm (Portable 68k Assembler)"
    },
    "SGDK": {
        "url": "https://github.com/stephane-d/sgdk.git",
        "note": "SGDK (Sega Genesis Development Kit)"
    },
    "PVSNESLib": {
        "url": "https://github.com/alekmaul/pvsneslib.git",
        "note": "PVSnesLib (SNES C Development Kit)"
    }
}

QUEUE_FILE = "runtime/card_queue.json"
ROOT_DIR = "oss_sovereignty/sys_16_16Bit_Revolution"

def mint_16bit_cards():
    print(">> MINTING 16-BIT REVOLUTION CARDS...")
    
    new_cards = []
    timestamp = datetime.now().isoformat()
    
    for sys_name, data in BIT16_MAP.items():
        # Determine target path
        target_path = None
        for root, dirs, files in os.walk(ROOT_DIR):
            if sys_name in dirs:
                target_path = os.path.join(root, sys_name)
                break
        
        if not target_path:
            # Fallback
            target_path = f"{ROOT_DIR}/00_Uncategorized/{sys_name}"
            if not os.path.exists(target_path):
                os.makedirs(target_path, exist_ok=True)

        # Create Card
        card = {
            "id": f"sov_16bit_{sys_name.lower()}",
            "description": f"16-BIT: Assimilate {sys_name} ({data['note']})",
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
        
    print(f">> Minted {len(new_cards)} 16-BIT cards.")

if __name__ == "__main__":
    mint_16bit_cards()
