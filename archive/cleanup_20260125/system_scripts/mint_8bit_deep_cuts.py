import json
import os
from datetime import datetime

# Mapping of High-Fidelity 8-Bit Systems
DEEP_CUTS_MAP = {
    "Visual6502": {
        "url": "https://github.com/trebonian/visual6502.git",
        "note": "Transistor-level 6502 Simulation",
        "category": "01_Emulators"
    },
    "puNES": {
        "url": "https://github.com/punes-emu/punes.git",
        "note": "Cycle-Accurate NES Emulator",
        "category": "01_Emulators"
    },
    "MAME": {
        "url": "https://github.com/mamedev/mame.git",
        "note": "Multi Arcade Machine Emulator (Core)",
        "category": "01_Emulators"
    },
    "ZX_Spectrum_Verilog": {
        "url": "https://github.com/AtlasFPGA/zx.git",
        "note": "Verilog FPGA Implementation of ZX Spectrum",
        "category": "03_Hardware_HDL" # New Category
    },
    "C64_VHDL": {
        "url": "https://github.com/ovalcode/c64fpga.git",
        "note": "VHDL/Verilog FPGA Implementation of C64",
        "category": "03_Hardware_HDL" # New Category
    },
    "WLA_DX": {
        "url": "https://github.com/vhelin/wla-dx.git",
        "note": "WLA DX Multi-Target Assembler",
        "category": "02_Toolchains"
    }
}

QUEUE_FILE = "runtime/card_queue.json"
ROOT_DIR = "oss_sovereignty/sys_15_8Bit_Renaissance"

def mint_deep_cuts():
    print(">> MINTING 8-BIT DEEP CUT CARDS (FIDELITY FOCUS)...")
    
    new_cards = []
    timestamp = datetime.now().isoformat()
    
    for sys_name, data in DEEP_CUTS_MAP.items():
        # Determine target path, creating new category if needed
        cat_path = os.path.join(ROOT_DIR, data["category"])
        if not os.path.exists(cat_path):
            os.makedirs(cat_path)
            
        target_path = os.path.join(cat_path, sys_name)
        if not os.path.exists(target_path):
            os.makedirs(target_path)

        # Create Card
        card = {
            "id": f"sov_8bit_deep_{sys_name.lower()}",
            "description": f"8-BIT DEEP: Assimilate {sys_name} ({data['note']})",
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
        
    print(f">> Minted {len(new_cards)} DEEP CUT cards.")

if __name__ == "__main__":
    mint_deep_cuts()
