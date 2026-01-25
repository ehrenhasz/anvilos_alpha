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
    
    print("--- PHASE 3: Memory Management ---")
    
    # Ensure directories exist
    submit_card(conn, "setup_mm_dirs", f"mkdir -p {KERNEL_ROOT}/kernel/mm")
    
    # 1. physical_memory_map (e820)
    e820_code = "struct E820Entry{base:u64,len:u64,typ:u32,ext:u32}struct MemMap{entries:[E820Entry;128],count:usize}impl MemMap{fn parse(p:u64,c:usize)->Self{let mut m=MemMap{entries:[E820Entry{base:0,len:0,typ:0,ext:0};128],count:c};unsafe{core::ptr::copy_nonoverlapping(p as*const E820Entry,m.entries.as_mut_ptr(),c)};m}}"
    cmd_e820 = f"echo '{e820_code}' > {KERNEL_ROOT}/kernel/mm/e820.anv"
    submit_card(conn, "physical_memory_map", cmd_e820)
    
    # 2. paging_init
    paging_code = "type PageTable=[u64;512];static mut PML4:PageTable=[0;512];static mut PDPT:PageTable=[0;512];fn init_paging(){unsafe{PML4[0]=(PDPT.as_ptr()as u64)|3;for i in 0..512{PDPT[i]=(i as u64*0x40000000)|0x83};asm!(\"mov %rax, %cr3\",in(\"rax\")PML4.as_ptr()as u64)}}"
    cmd_paging = f"echo '{paging_code}' > {KERNEL_ROOT}/kernel/mm/paging.anv"
    submit_card(conn, "paging_init", cmd_paging)
    
    # 3. kernel_heap_allocator
    heap_code = "struct Heap{start:u64,size:u64,used:u64}impl Heap{fn alloc(&mut self,size:usize)->u64{if self.used+size as u64>self.size{0}else{let p=self.start+self.used;self.used+=size as u64;p}}}"
    cmd_heap = f"echo '{heap_code}' > {KERNEL_ROOT}/kernel/mm/heap.anv"
    submit_card(conn, "kernel_heap_allocator", cmd_heap)

    conn.commit()
    conn.close()

if __name__ == "__main__":
    main()
