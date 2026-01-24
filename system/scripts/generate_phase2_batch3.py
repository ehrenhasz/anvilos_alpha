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
    
    print("--- PHASE 2: Batch 3 (PIC & Exceptions) ---")
    
    # 7. pic_remap
    # Remap PIC1 to 0x20 and PIC2 to 0x28
    pic_code = "struct Pic{m:u16,s:u16}impl Pic{fn remap(){unsafe{asm!(\"outb %al, $0x20;outb %al, $0xa0;outb %al, $0x21;outb %al, $0xa1;outb %al, $0x21;outb %al, $0xa1;outb %al, $0x21;outb %al, $0xa1\",in(\"al\")0x11,options(nostack));asm!(\"outb %al, $0x21\",in(\"al\")0x20);asm!(\"outb %al, $0xa1\",in(\"al\")0x28);asm!(\"outb %al, $0x21\",in(\"al\")4);asm!(\"outb %al, $0xa1\",in(\"al\")2);asm!(\"outb %al, $0x21\",in(\"al\")1);asm!(\"outb %al, $0xa1\",in(\"al\")1);asm!(\"outb %al, $0x21\",in(\"al\")0);asm!(\"outb %al, $0xa1\",in(\"al\")0);}}}} "
    cmd_pic = f"echo '{pic_code}' > {KERNEL_ROOT}/kernel/cpu/pic.anv"
    submit_card(conn, "pic_remap", cmd_pic)
    
    # 8. cpu_exceptions
    # Generic fault handler. Panic on fault.
    exc_code = "fn fault_handler(i:u8,e:u64){let vga=Vga::new();vga.write(0,0x46);vga.write(1,0x41);vga.write(2,0x55);vga.write(3,0x4c);vga.write(4,0x54);loop{unsafe{asm!(\"hlt\")}}}"
    cmd_exc = f"echo '{exc_code}' > {KERNEL_ROOT}/kernel/cpu/exceptions.anv"
    submit_card(conn, "cpu_exceptions", cmd_exc)

    conn.commit()
    conn.close()

if __name__ == "__main__":
    main()
