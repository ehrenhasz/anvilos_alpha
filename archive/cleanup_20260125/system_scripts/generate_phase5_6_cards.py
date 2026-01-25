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
    
    print("--- PHASE 5 & 6: I/O, FS & Userland ---")
    
    # Ensure directories exist
    submit_card(conn, "setup_fs_dirs", f"mkdir -p {KERNEL_ROOT}/kernel/fs {KERNEL_ROOT}/kernel/syscalls {KERNEL_ROOT}/kernel/binfmt {KERNEL_ROOT}/tools")
    
    # Phase 5
    submit_card(conn, "ps2_keyboard_poller", f"echo 'fn kbd_read()->u8{{unsafe{{let mut b:u8;asm!(\"inb %dx, %al\",out(\"al\")b,in(\"dx\")0x60as u16);b}}}}' > {KERNEL_ROOT}/kernel/drivers/keyboard.anv")
    submit_card(conn, "vfs_trait_definition", f"echo 'trait Vfs{{fn read(&self,p:&str)->Vec<u8>;fn write(&mut self,p:&str,d:&[u8]);}}' > {KERNEL_ROOT}/kernel/fs/vfs.anv")
    submit_card(conn, "initramfs_parser", f"echo 'struct CpioHeader{{m:u16,d:u16,i:u16,mde:u16,u:u16,g:u16,nl:u16,mt:u32,sz:u32,nm:u16}}' > {KERNEL_ROOT}/kernel/fs/initramfs.anv")
    
    # Phase 6
    submit_card(conn, "syscall_dispatcher", f"echo 'fn syscall_handler(n:u64,a1:u64,a2:u64)->u64{{match n{{1=>{{/*print*/0}}2=>{{/*exit*/0}}_=>0}}}}' > {KERNEL_ROOT}/kernel/syscalls/handler.anv")
    submit_card(conn, "elf_loader", f"echo 'struct ElfHeader{{m:[u8;4],cl:u8,d:u8,v:u8,os:u8,abiv:u8,p:[u8;7],t:u16,mac:u16,ver:u32,e:u64,ph:u64,sh:u64,f:u32,hs:u16,ps:u16,pc:u16,ss:u16,sc:u16,si:u16}}' > {KERNEL_ROOT}/kernel/binfmt/elf.anv")
    submit_card(conn, "man_page_viewer", f"echo 'fn main(){{let args:Vec<String>=std::env::args().collect();if args.len()>1{{let c=std::fs::read_to_string(&args[1]).unwrap();println!(\"{{}}\",c);}}}}' > {KERNEL_ROOT}/tools/man_pager.anv")

    conn.commit()
    conn.close()

if __name__ == "__main__":
    main()
