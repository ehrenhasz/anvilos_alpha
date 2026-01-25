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
    
    print("--- PHASE 2: Batch 2 (Drivers & CPU) ---")
    
    # Ensure directories exist
    submit_card(conn, "setup_kernel_dirs", f"mkdir -p {KERNEL_ROOT}/kernel/drivers {KERNEL_ROOT}/kernel/cpu")
    
    # 3. vga_buffer_driver
    # Address 0xb8000. Minified .anv (Anvil/Rust-like) 
    vga_code = "struct Vga{p:*mut u8}impl Vga{fn new()->Self{Vga{p:0xb8000 as*mut u8}}fn write(&self,i:usize,b:u8){unsafe{*self.p.add(i*2)=b;*self.p.add(i*2+1)=0x0f}}}"
    cmd_vga = f"echo '{vga_code}' > {KERNEL_ROOT}/kernel/drivers/vga.anv"
    submit_card(conn, "vga_buffer_driver", cmd_vga)
    
    # 4. serial_port_shim
    # Port 0x3F8 (COM1).
    serial_code = "struct Serial{p:u16}impl Serial{fn new(p:u16)->Self{Serial{p}}fn write(&self,b:u8){unsafe{asm!(\"outb %al, %dx\",in(\"al\")b,in(\"dx\")self.p,options(nostack,nomem))}}}"
    cmd_serial = f"echo '{serial_code}' > {KERNEL_ROOT}/kernel/drivers/serial.anv"
    submit_card(conn, "serial_port_shim", cmd_serial)
    
    # 5. gdt_rewrite
    # Minimal GDT.
    gdt_code = "struct GdtEntry{l:u16,b_l:u16,b_m:u8,a:u8,f:u8,b_h:u8}struct GdtPtr{limit:u16,base:u64}static mut GDT:[GdtEntry;3]=[GdtEntry{l:0,b_l:0,b_m:0,a:0,f:0,b_h:0},GdtEntry{l:0,b_l:0,b_m:0,a:0x9a,f:0xaf,b_h:0},GdtEntry{l:0,b_l:0,b_m:0,a:0x92,f:0xaf,b_h:0}];"
    cmd_gdt = f"echo '{gdt_code}' > {KERNEL_ROOT}/kernel/cpu/gdt.anv"
    submit_card(conn, "gdt_rewrite", cmd_gdt)
    
    # 6. idt_structure
    idt_code = "struct IdtEntry{o_l:u16,s:u16,i:u8,t:u8,o_m:u16,o_h:u32,r:u32}struct IdtPtr{l:u16,b:u64}static mut IDT:[IdtEntry;256]=[IdtEntry{o_l:0,s:0,i:0,t:0,o_m:0,o_h:0,r:0};256];"
    cmd_idt = f"echo '{idt_code}' > {KERNEL_ROOT}/kernel/cpu/idt.anv"
    submit_card(conn, "idt_structure", cmd_idt)

    conn.commit()
    conn.close()

if __name__ == "__main__":
    main()
