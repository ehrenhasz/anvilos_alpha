import os
import json
import sqlite3
import uuid

# --- CONFIG ---
PROJECT_ROOT = os.getcwd()
DB_PATH = os.path.join(PROJECT_ROOT, "data", "cortex.db")
OUTPUT_FILE = os.path.join(PROJECT_ROOT, "forge_directives.py")

# --- PHASE 2 & 3 DIRECTIVES (The Fixed Stack) ---
# These are the manual steps we verified work.
MANUAL_STEPS = [
    # PHASE 2: RECOVERY & ASSEMBLY
    {"op": "SYS_CMD", "pld": {"cmd": "mkdir -p build_artifacts/rootfs_stage/bin build_artifacts/rootfs_stage/sbin build_artifacts/rootfs_stage/usr/bin build_artifacts/rootfs_stage/usr/sbin iso/boot/grub", "desc": "Setup: Create directory structure."}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp -f oss_sovereignty/busybox-1.36.1/busybox build_artifacts/rootfs_stage/bin/busybox && chmod +x build_artifacts/rootfs_stage/bin/busybox", "desc": "Install: Busybox (Verified Path)."}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp -f oss_sovereignty/zfs-2.2.2/zfs build_artifacts/rootfs_stage/sbin/zfs && chmod +x build_artifacts/rootfs_stage/sbin/zfs", "desc": "Install: ZFS Binary (Verified Path)."}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp -f oss_sovereignty/zfs-2.2.2/zpool build_artifacts/rootfs_stage/sbin/zpool && chmod +x build_artifacts/rootfs_stage/sbin/zpool", "desc": "Install: Zpool Binary (Verified Path)."}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp -f build_artifacts/bzImage iso/boot/bzImage", "desc": "Install: Kernel (Verified Path)."}},
    
    # PHASE 3: TRANSMUTATION (The Law)
    {"op": "SYS_CMD", "pld": {"cmd": "mkdir -p build_artifacts/rootfs_stage/lib/anvil/microjson build_artifacts/rootfs_stage/lib/anvil/services build_artifacts/rootfs_stage/lib/anvil/agents build_artifacts/rootfs_stage/lib/anvil/cortex build_artifacts/rootfs_stage/lib/anvil/scripts", "desc": "Setup: Create Anvil Library Hierarchy."}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/synapse.py", "desc": "Transmute: Synapse"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/synapse.mpy build_artifacts/rootfs_stage/lib/anvil/synapse.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/mainframe_init.py", "desc": "Transmute: Init"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/mainframe_init.mpy build_artifacts/rootfs_stage/lib/anvil/init.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/forge.py", "desc": "Transmute: Forge"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/forge.mpy build_artifacts/rootfs_stage/lib/anvil/forge.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/memories.py", "desc": "Transmute: Memories"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/memories.mpy build_artifacts/rootfs_stage/lib/anvil/memories.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/logger.py", "desc": "Transmute: Logger"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/logger.mpy build_artifacts/rootfs_stage/lib/anvil/logger.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/discovery_client.py", "desc": "Transmute: Discovery Client"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/discovery_client.mpy build_artifacts/rootfs_stage/lib/anvil/discovery_client.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/mainframe_client.py", "desc": "Transmute: Mainframe Client"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/mainframe_client.mpy build_artifacts/rootfs_stage/lib/anvil/mainframe_client.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/mainframe_monitor.py", "desc": "Transmute: Mainframe Monitor"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/mainframe_monitor.mpy build_artifacts/rootfs_stage/lib/anvil/mainframe_monitor.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/mainframe_tools.py", "desc": "Transmute: Mainframe Tools"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/mainframe_tools.mpy build_artifacts/rootfs_stage/lib/anvil/mainframe_tools.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/dashboard_server.py", "desc": "Transmute: Dashboard Server"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/dashboard_server.mpy build_artifacts/rootfs_stage/lib/anvil/dashboard_server.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/microjson/codec.py", "desc": "Transmute: MicroJSON Codec"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/microjson/codec.mpy build_artifacts/rootfs_stage/lib/anvil/microjson/codec.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/microjson/__init__.py", "desc": "Transmute: MicroJSON Init"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/microjson/__init__.mpy build_artifacts/rootfs_stage/lib/anvil/microjson/__init__.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/cortex/db_interface.py", "desc": "Transmute: Cortex DB"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/cortex/db_interface.mpy build_artifacts/rootfs_stage/lib/anvil/cortex/db_interface.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/cortex/__init__.py", "desc": "Transmute: Cortex Init"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/cortex/__init__.mpy build_artifacts/rootfs_stage/lib/anvil/cortex/__init__.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/agents/spy.py", "desc": "Transmute: Agent Spy"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/agents/spy.mpy build_artifacts/rootfs_stage/lib/anvil/agents/spy.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/agents/witch.py", "desc": "Transmute: Agent Witch"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/agents/witch.mpy build_artifacts/rootfs_stage/lib/anvil/agents/witch.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/services/card_reader.py", "desc": "Transmute: Card Reader"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/services/card_reader.mpy build_artifacts/rootfs_stage/lib/anvil/services/card_reader.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/services/interpretation.py", "desc": "Transmute: Interpretation"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/services/interpretation.mpy build_artifacts/rootfs_stage/lib/anvil/services/interpretation.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/services/interceptor_bridge.py", "desc": "Transmute: Interceptor Bridge"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/services/interceptor_bridge.mpy build_artifacts/rootfs_stage/lib/anvil/services/interceptor_bridge.anv"}},
    
    {"op": "forge_py_artifact", "pld": {"src": "src/anvilos/scripts/reset_and_test.py", "desc": "Transmute: Reset Script"}},
    {"op": "SYS_CMD", "pld": {"cmd": "cp build_artifacts/objects/src/anvilos/scripts/reset_and_test.mpy build_artifacts/rootfs_stage/lib/anvil/scripts/reset_and_test.anv"}},
    
    # FINAL PACKING
    {"op": "SYS_CMD", "pld": {"cmd": "cd build_artifacts/rootfs_stage && find . | cpio -o -H newc | gzip > ../../iso/boot/initramfs.cpio.gz", "desc": "Repack: Initramfs"}},
    {"op": "FILE_WRITE", "pld": {"path": "iso/boot/grub/grub.cfg", "content": "set default=0\nset timeout=5\nmenuentry \"AnvilOS\" {\n    linux /boot/bzImage\n    initrd /boot/initramfs.cpio.gz\n}", "desc": "Config: GRUB"}},
    {"op": "SYS_CMD", "pld": {"cmd": "grub-mkrescue -o build_artifacts/anvilos.iso iso", "desc": "Final: ISO Creation"}}
]

def generate_verification_cards():
    print("Scanning oss_sovereignty...")
    cards = []
    base_dir = os.path.join(PROJECT_ROOT, "oss_sovereignty")
    
    for root, dirs, files in os.walk(base_dir):
        for file in files:
            # Skip build artifacts inside source
            if "build/" in root or ".git" in root: continue
            
            abs_path = os.path.join(root, file)
            rel_path = os.path.relpath(abs_path, PROJECT_ROOT)
            
            cards.append({
                "op": "verify_file_integrity",
                "pld": {
                    "path": rel_path,
                    "desc": f"Verify: {os.path.basename(file)}"
                }
            })
    return cards

def main():
    # 1. Generate Verification Cards (The 41k)
    verify_cards = generate_verification_cards()
    print(f"Generated {len(verify_cards)} verification cards.")
    
    # 2. Combine with Manual Steps
    full_stack = verify_cards + MANUAL_STEPS
    print(f"Total Stack Size: {len(full_stack)}")
    
    # 3. Write to forge_directives.py (for record)
    # We write a simplified version to avoid creating a 20MB python file that crashes editors.
    # Actually, the user asked to "update forge_directives.py". I will write it all.
    print(f"Writing to {OUTPUT_FILE}...")
    with open(OUTPUT_FILE, "w") as f:
        f.write("# FULL STACK GENERATED BY AIMEAT\n")
        f.write("FULL_STACK = [\n")
        for i, card in enumerate(full_stack):
            card['seq'] = i + 1
            f.write(f"    {json.dumps(card)},\n")
        f.write("]\n")
        
    # 4. Inject into DB (Batched)
    print("Injecting into Mainframe...")
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    
    # Clear old stack
    cursor.execute("DELETE FROM card_stack")
    
    batch_size = 50
    for i in range(0, len(full_stack), batch_size):
        batch = full_stack[i:i+batch_size]
        print(f"Injecting batch {i} to {i+len(batch)}...")
        
        for card in batch:
            card_id = str(uuid.uuid4())
            seq = card['seq']
            op = card['op']
            pld = card['pld']
            pld['_source'] = 'FORGE' # Sign for Warden
            
            cursor.execute('''
                INSERT INTO card_stack (id, seq, op, pld, stat, agent_id, timestamp)
                VALUES (?, ?, ?, ?, 0, 'FORGE', CURRENT_TIMESTAMP)
            ''', (card_id, seq, op, json.dumps(pld)))
        
        conn.commit()
        
    conn.close()
    print("INJECTION COMPLETE.")

if __name__ == "__main__":
    main()
