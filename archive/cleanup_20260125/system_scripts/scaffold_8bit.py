import os
import json
from datetime import datetime

ROOT = "oss_sovereignty"
SYSTEM_NAME = "sys_15_8Bit_Renaissance"

STRUCTURE = {
    "description": "THE RENAISSANCE: 8-bit Emulation and Toolchains.",
    "categories": {
        "01_Emulators": [
            "NES", "GameBoy", "Commodore_64", "ZX_Spectrum", 
            "Apple_II", "Atari_8Bit", "MSX", "BBC_Micro"
        ],
        "02_Toolchains": [
            "cc65", "z88dk", "SDCC", "RGBDS", "ASM6"
        ]
    }
}

def scaffold_8bit():
    print(f">> SCAFFOLDING {SYSTEM_NAME}...")
    sys_path = os.path.join(ROOT, SYSTEM_NAME)
    
    if not os.path.exists(sys_path):
        os.makedirs(sys_path)
        
    with open(os.path.join(sys_path, "metadata.json"), "w") as f:
        json.dump({
            "description": STRUCTURE["description"],
            "created_at": datetime.now().isoformat(),
            "status": "scaffolded"
        }, f, indent=2)

    for cat, items in STRUCTURE["categories"].items():
        cat_path = os.path.join(sys_path, cat)
        if not os.path.exists(cat_path):
            os.makedirs(cat_path)
            
        for item in items:
            item_path = os.path.join(cat_path, item)
            if not os.path.exists(item_path):
                os.makedirs(item_path)
                os.makedirs(os.path.join(item_path, "source"))
                os.makedirs(os.path.join(item_path, "build"))
                
                with open(os.path.join(item_path, "metadata.json"), "w") as f:
                    json.dump({
                        "id": f"{SYSTEM_NAME}.{item}",
                        "name": item,
                        "category": cat,
                        "mandate": "PRESERVE AND EMULATE"
                    }, f, indent=2)
                    
    print(">> SCAFFOLDING COMPLETE.")

if __name__ == "__main__":
    scaffold_8bit()
