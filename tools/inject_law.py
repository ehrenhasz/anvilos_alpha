import sqlite3
import json
import uuid
import time
import os
DB_PATH = "data/cortex.db"
def forge_law():
    conn = sqlite3.connect(DB_PATH)
    cards = [
        (700, "FILE_WRITE", {"path": "src/native/kernel.h", "content": "#ifndef KERNEL_H\n#define KERNEL_H\n#include <stdint.h>\n#include <stddef.h>\n#endif"}),
        (701, "SYS_CMD", {"cmd": "python3 oss_sovereignty/sys_09_Anvil/source/anvil.py transpile src/native/init.anv src/native/init.c", "desc": "7. LAW: TRANSPILE_INIT"}),
        (702, "SYS_CMD", {"cmd": "gcc -static -nostdlib -Isrc/native src/native/init.c -o build_artifacts/rootfs_stage/init", "desc": "7. LAW: COMPILE_SOVEREIGN_INIT"}),
        (703, "SYS_CMD", {"cmd": "cd build_artifacts/rootfs_stage && find . | cpio -o -H newc | gzip > ../iso/boot/initramfs.cpio.gz && cd ../.. && grub-mkrescue -o anvilos-law.iso build_artifacts/iso", "desc": "7. LAW: REPACK_ISO"})
    ]
    for seq, op, pld in cards:
        card_id = str(uuid.uuid4())
        conn.execute("INSERT INTO card_stack (id, seq, op, pld, stat, timestamp) VALUES (?, ?, ?, ?, 0, ?)",
                     (card_id, seq, op, json.dumps(pld), time.time()))
    conn.commit()
    conn.close()
    print("Law Injected. Cards 700-703 are live.")
if __name__ == "__main__":
    forge_law()
