import os
import json
import uuid
from datetime import datetime

OSS_DIR = "oss_sovereignty"
QUEUE_DIR = "/mnt/anvil_temp/cards/queue"

# Map folders to Git URLs
REPO_MAP = {
    "bld_01_GCC": "https://gcc.gnu.org/git/gcc.git",
    "bld_02_Musl_Libc": "https://git.musl-libc.org/git/musl",
    "bld_03_Xorriso": "https://dev.lovelyhq.com/libburnia/libisoburn.git",
    "bld_04_CPIO": "https://git.savannah.gnu.org/git/cpio.git",
    "bld_05_Gzip": "https://git.savannah.gnu.org/git/gzip.git",
    "bld_06_MicroPython": "https://github.com/micropython/micropython.git",
    "ctx_01__google_genai": "https://github.com/google/generative-ai-js.git",
    "ctx_02__google_generative_ai": "https://github.com/google/generative-ai-js.git", 
    "ctx_03_dotenv": "https://github.com/motdotla/dotenv.git",
    "ctx_04_react": "https://github.com/facebook/react.git",
    "ctx_05_react_dom": "https://github.com/facebook/react.git",
    "ctx_06_recharts": "https://github.com/recharts/recharts.git",
    "ctx_07_vite": "https://github.com/vitejs/vite.git",
    "ctx_08_typescript": "https://github.com/microsoft/TypeScript.git",
    "ctx_09_tailwindcss": "https://github.com/tailwindlabs/tailwindcss.git",
    "os_01_Linux_Kernel": "https://github.com/torvalds/linux.git",
    "os_02_BusyBox": "https://git.busybox.net/busybox",
    "os_03_Syslinux": "https://repo.or.cz/syslinux.git",
    "sys_02_Bash": "https://git.savannah.gnu.org/git/bash.git",
    "sys_03_Python": "https://github.com/python/cpython.git",
    "sys_04_Node_js": "https://github.com/nodejs/node.git",
    "sys_05_QEMU_KVM": "https://github.com/qemu/qemu.git",
    "sys_06_OpenSSH": "https://github.com/openssh/openssh-portable.git",
    "sys_07_Btop": "https://github.com/aristocratos/btop.git",
    "sys_08_Aerc": "https://git.sr.ht/~rjarry/aerc"
}

def get_directories(path):
    if not os.path.exists(path):
        return []
    return [d for d in os.listdir(path) if os.path.isdir(os.path.join(path, d))]

def create_card(folder, url):
    card_id = str(uuid.uuid4())[:8]
    
    # Task Description:
    # 1. Clone repo into the specific folder (assumes empty).
    # 2. Remove the .git directory to sever ties.
    description = (
        f"CLONE_AND_SEVER: Target '{OSS_DIR}/{folder}'. "
        f"1. GIT CLONE: Clone '{url}' into '{OSS_DIR}/{folder}'. "
        f"2. SEVER: Remove '{OSS_DIR}/{folder}/.git' directory. "
        f"3. VERIFY: Ensure files exist."
    )
    
    card = {
        "id": card_id,
        "description": description,
        "status": "todo",
        "created_at": datetime.now().isoformat()
    }
    
    filename = os.path.join(QUEUE_DIR, f"{card_id}.json")
    with open(filename, 'w') as f:
        json.dump(card, f, indent=2)
    print(f"Created card {card_id} for {folder}")

def main():
    dirs = get_directories(OSS_DIR)
    print(f"Found {len(dirs)} directories in {OSS_DIR}")
    
    count = 0
    for d in dirs:
        if d in REPO_MAP:
            create_card(d, REPO_MAP[d])
            count += 1
        else:
            print(f"Skipping {d}: No repo URL mapped.")
            
    print(f"Successfully generated {count} clone cards.")

if __name__ == "__main__":
    main()
