import json
import os
from datetime import datetime

# Mapping of 8-Bit Systems
BIT8_MAP = {
    # --- EMULATORS ---
    "NES": {
        "url": "https://github.com/TASEmulators/fceux.git",
        "note": "FCEUX (NES Emulator)"
    },
    "GameBoy": {
        "url": "https://github.com/gb-archive/gambatte.git",
        "note": "Gambatte (Accuracy-focused GB)"
    },
    "Commodore_64": {
        "url": "https://git.code.sf.net/p/vice-emu/code",
        "note": "VICE (Versatile Commodore Emulator)"
    },
    "ZX_Spectrum": {
        "url": "https://git.code.sf.net/p/fuse-emulator/fuse",
        "note": "Fuse (Free Unix Spectrum Emulator)"
    },
    "Apple_II": {
        "url": "https://github.com/linappleii/linapple.git",
        "note": "LinApple (Apple II Emulator)"
    },
    "Atari_8Bit": {
        "url": "https://github.com/atari800/atari800.git",
        "note": "Atari800 Emulator"
    },
    "MSX": {
        "url": "https://github.com/openMSX/openMSX.git",
        "note": "openMSX"
    },
    "BBC_Micro": {
        "url": "https://github.com/stardot/beebem-windows.git",
        "note": "BeebEm (Windows Port Source)"
    },
    
    # --- TOOLCHAINS ---
    "cc65": {
        "url": "https://github.com/cc65/cc65.git",
        "note": "cc65 (6502 C Compiler/Assembler)"
    },
    "z88dk": {
        "url": "https://github.com/z88dk/z88dk.git",
        "note": "z88dk (Z80 Development Kit)"
    },
    "SDCC": {
        "url": "https://git.code.sf.net/p/sdcc/code",
        "note": "SDCC (Small Device C Compiler)"
    },
    "RGBDS": {
        "url": "https://github.com/gbdev/rgbds.git",
        "note": "RGBDS (Rednex Game Boy Development System)"
    },
    "ASM6": {
        "url": "https://github.com/freem/asm6f.git",
        "note": "ASM6f (Fork of ASM6 for NES)"
    }
}

QUEUE_FILE = "runtime/card_queue.json"
ROOT_DIR = "oss_sovereignty/sys_15_8Bit_Renaissance"

def mint_8bit_cards():
    print(">> MINTING 8-BIT RENAISSANCE CARDS...")
    
    new_cards = []
    timestamp = datetime.now().isoformat()
    
    for sys_name, data in BIT8_MAP.items():
        # Determine target path by walking the directory
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
            "id": f"sov_8bit_{sys_name.lower()}",
            "description": f"8-BIT: Assimilate {sys_name} ({data['note']})",
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
        
    print(f">> Minted {len(new_cards)} 8-BIT cards.")

if __name__ == "__main__":
    mint_8bit_cards()
