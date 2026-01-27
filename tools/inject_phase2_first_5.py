import os
import sys
import json
import uuid
import time
import sqlite3

PROJECT_ROOT = os.getcwd()

# Path injection
sys.path.append(os.path.join(PROJECT_ROOT, "vendor"))
for venv_dir in ["venv", ".venv"]:
    lib_path = os.path.join(PROJECT_ROOT, venv_dir, "lib")
    if os.path.exists(lib_path):
        for py_dir in os.listdir(lib_path):
            if py_dir.startswith("python"):
                site_pkg = os.path.join(lib_path, py_dir, "site-packages")
                if os.path.exists(site_pkg):
                    sys.path.append(site_pkg)

sys.path.append(PROJECT_ROOT)

from architect_daemon import architect_recipe, DB

def clear_deck():
    print("Clearing deck...")
    conn = sqlite3.connect("data/cortex.db")
    conn.execute("DELETE FROM card_stack")
    conn.commit()
    conn.close()

def inject_first_5():
    clear_deck()
    
    # Define the 5 goals with explicit context
    goals = [
        f"Card 101: clean_linux_source. In {PROJECT_ROOT}/oss_sovereignty/linux-6.6.14, run 'make mrproper'.",
        f"Card 102: clean_busybox_source. In {PROJECT_ROOT}/oss_sovereignty/busybox-1.36.1, run 'make mrproper'.",
        f"Card 200: config_busybox_static. In {PROJECT_ROOT}/oss_sovereignty/busybox-1.36.1, generate a default config (defconfig) and then set CONFIG_STATIC=y in .config.",
        f"Card 201: build_busybox. In {PROJECT_ROOT}/oss_sovereignty/busybox-1.36.1, run 'make -j$(nproc)'.",
        f"Card 202: install_busybox. In {PROJECT_ROOT}/oss_sovereignty/busybox-1.36.1, run 'make install' with DESTDIR={PROJECT_ROOT}/build_artifacts/rootfs_stage."
    ]

    print(f"Injecting {len(goals)} cards...")
    
    seq_counter = 0
    for goal in goals:
        print(f"Architecting: {goal}")
        recipe = architect_recipe(goal)
        
        if recipe:
            for card in recipe:
                card_id = str(uuid.uuid4())
                print(f"  [+] Pushing Card {seq_counter}: {card['op']}")
                DB.push_card(card_id, seq_counter, card['op'], card['pld'])
                seq_counter += 1
        else:
            print(f"  [-] Failed to architect: {goal}")

    print("Injection complete.")

if __name__ == "__main__":
    inject_first_5()
