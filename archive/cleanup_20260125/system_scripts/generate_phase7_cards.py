#!/usr/bin/env python3
import sqlite3
import json
import uuid
import os

DB_PATH = "/var/lib/anvilos/db/cortex.db"
KERNEL_ROOT = "oss_sovereignty/sys_01_Linux_Kernel/source"

def submit_card(conn, context, command):
    correlation_id = f"anvil-{uuid.uuid4().hex[:8]}"
    card_payload = {
        "context": context,
        "details": command,
        "description": f"SYSTEM_OP: {context}",
        "source": "LADYSMITH_DECOMPOSER",
        "instruction": "SYSTEM_OP",
        "format": "shell"
    }
    conn.execute("""
        INSERT INTO jobs (correlation_id, idempotency_key, priority, cost_center, payload, status)
        VALUES (?, ?, ?, ?, ?, 'PENDING')
    """, (correlation_id, f"op-{correlation_id}", 50, "OPS", json.dumps(card_payload)))
    print(f"Queued: {context}")

def main():
    conn = sqlite3.connect(DB_PATH)
    
    print("--- PHASE 7: Final Cleanup ---")
    
    # 1. strip_symbols
    submit_card(conn, "strip_symbols", f"sed -i 's/STRIP_DEBUG=0/STRIP_DEBUG=1/g' {KERNEL_ROOT}/anvil.toml")
    
    # 2. final_link_script
    linker_ld = "ENTRY(_start)SECTIONS{.=1M;.text:{*(.multiboot_header)*(.text)}.rodata:{*(.rodata)}.data:{*(.data)}.bss:{*(.bss)}}"
    cmd_linker = f"echo '{linker_ld}' > {KERNEL_ROOT}/linker.ld"
    submit_card(conn, "final_link_script", cmd_linker)
    
    # 3. generate_iso_recipe
    iso_recipe = "#!/bin/bash\nmkdir -p isodir/boot/grub\ncp kernel.bin isodir/boot/kernel.bin\necho 'menuentry \"Anvil OS\" {multiboot2 /boot/kernel.bin; boot}' > isodir/boot/grub/grub.cfg\ngrub-mkrescue -o anvil.iso isodir"
    cmd_iso = f"echo '{iso_recipe}' > {KERNEL_ROOT}/build_iso.sh && chmod +x {KERNEL_ROOT}/build_iso.sh"
    submit_card(conn, "generate_iso_recipe", cmd_iso)

    conn.commit()
    conn.close()

if __name__ == "__main__":
    main()
