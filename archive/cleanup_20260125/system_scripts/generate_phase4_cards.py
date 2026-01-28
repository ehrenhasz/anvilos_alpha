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
    
    print("--- PHASE 4: Process Management ---")
    
    # Ensure directories exist
    submit_card(conn, "setup_sched_dirs", f"mkdir -p {KERNEL_ROOT}/kernel/sched")
    
    # 1. context_switch_asm
    switch_asm = ".global switch_task;switch_task:;push %rbp;push %rbx;push %r12;push %r13;push %r14;push %r15;mov %rsp, (%rdi);mov (%rsi), %rsp;pop %r15;pop %r14;pop %r13;pop %r12;pop %rbx;pop %rbp;ret"
    cmd_switch = f"echo '{switch_asm}' > {KERNEL_ROOT}/kernel/sched/switch.S"
    submit_card(conn, "context_switch_asm", cmd_switch)
    
    # 2. process_struct
    task_code = "struct Task{rsp:u64,cr3:u64,pid:u32,state:u8}static mut TASKS:[Task;64]=[Task{rsp:0,cr3:0,pid:0,state:0};64];static mut CUR:usize=0;"
    cmd_task = f"echo '{task_code}' > {KERNEL_ROOT}/kernel/sched/task.anv"
    submit_card(conn, "process_struct", cmd_task)
    
    # 3. pit_scheduler
    pit_code = "fn pit_init(h:u16){unsafe{asm!(\"outb %al, $0x43\",in(\"al\")0x36);asm!(\"outb %al, $0x40\",in(\"al\")(h&0xff)as u8);asm!(\"outb %al, $0x40\",in(\"al\")(h>>8)as u8)}}"
    cmd_pit = f"echo '{pit_code}' > {KERNEL_ROOT}/kernel/drivers/pit.anv"
    submit_card(conn, "pit_scheduler", cmd_pit)

    conn.commit()
    conn.close()

if __name__ == "__main__":
    main()
