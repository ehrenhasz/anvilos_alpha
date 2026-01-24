import os
import json
from datetime import datetime

ROOT = "oss_sovereignty"

SYSTEMS = {
    "sys_17_32Bit_Era": {
        "description": "THE 32-BIT ERA: Consoles, Computers, and Toolchains.",
        "categories": {
            "01_Consoles": [
                "PlayStation_1", "Sega_Saturn", "Nintendo_64", "Dreamcast"
            ],
            "02_Computers": [
                "Windows_95_98_ME", "Windows_NT_2000_XP", "Mac_OS_Classic", "Linux_Early", "BeOS", "OS_9"
            ],
            "03_Toolchains": [
                "GCC_x86", "Visual_C_Plus_Plus", "PsyQ", "Katana_SDK"
            ]
        }
    },
    "sys_18_64Bit_Cloud": {
        "description": "THE 64-BIT & CLOUD ERA: Modern Systems, Cloud Platforms, and Virtualization.",
        "categories": {
            "01_Operating_Systems": [
                "Linux_Modern", "Windows_Modern", "macOS_Modern", "FreeBSD_Modern", "ChromeOS", "Android_Cloud", "iOS_Cloud"
            ],
            "02_Cloud_Platforms": [
                "AWS", "Azure", "GCP", "OpenStack", "Kubernetes", "Docker"
            ],
            "03_Virtualization": [
                "QEMU_KVM", "VirtualBox", "VMware", "Xen", "Hyper_V"
            ],
            "04_Languages_Runtimes": [
                "Rust_Modern", "Go_Modern", "Java_JVM", "DotNet_CLR", "Node_V8"
            ]
        }
    }
}

def scaffold_32_64bit():
    print(">> SCAFFOLDING 32-BIT AND 64-BIT SYSTEMS...")
    
    for sys_name, sys_data in SYSTEMS.items():
        sys_path = os.path.join(ROOT, sys_name)
        
        if not os.path.exists(sys_path):
            os.makedirs(sys_path)
            
        with open(os.path.join(sys_path, "metadata.json"), "w") as f:
            json.dump({
                "description": sys_data["description"],
                "created_at": datetime.now().isoformat(),
                "status": "scaffolded"
            }, f, indent=2)

        for cat, items in sys_data["categories"].items():
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
                            "id": f"{sys_name}.{item}",
                            "name": item,
                            "category": cat,
                            "mandate": "PRESERVE AND REIMPLEMENT"
                        }, f, indent=2)
                        
    print(">> SCAFFOLDING COMPLETE.")

if __name__ == "__main__":
    scaffold_32_64bit()
