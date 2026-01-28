import sqlite3
import json
import uuid
import os
DB_PATH = "/var/lib/anvilos/db/cortex.db"
KERNEL_ROOT = "oss_sovereignty/sys_01_Linux_Kernel/source"
def get_job_exists(conn, context):
    cur = conn.cursor()
    cur.execute("SELECT 1 FROM jobs WHERE payload LIKE ?", (f'%\"{context}\"%',))
    return cur.fetchone() is not None
def submit_card_deferred(conn, context, command, phase):
    if get_job_exists(conn, context):
        print(f"Skipping duplicate: {context}")
        return
    correlation_id = f"anvil-{uuid.uuid4().hex[:8]}"
    card_payload = {
        "context": context,
        "details": command,
        "description": f"SYSTEM_OP: {context} ({phase})",
        "source": "LADYSMITH_DECOMPOSER",
        "instruction": "SYSTEM_OP",
        "format": "shell"
    }
    pass
def main():
    conn = sqlite3.connect(DB_PATH)
    print("--- ANVIL KERNEL: FULL PROJECT CARD GENERATION (PHASES 3-7) ---")
    submit_card(conn, "setup_mm_dir", f"mkdir -p {KERNEL_ROOT}/kernel/mm")
    e820_code = "struct MemMap{b:u64,l:u64,t:u32}impl MemMap{fn parse(t:*mut u32)->Self{MemMap{b:0,l:0,t:0}}}"
    submit_card(conn, "physical_memory_map", f"echo '{e820_code}' > {KERNEL_ROOT}/kernel/mm/e820.anv")
    paging_code = "struct PageTable{e:[u64;512]}impl PageTable{fn map(&mut self,v:u64,p:u64,f:u64){self.e[((v>>12)&0x1ff)as usize]=p|f|1;}}"
    submit_card(conn, "paging_init", f"echo '{paging_code}' > {KERNEL_ROOT}/kernel/mm/paging.anv")
    heap_code = "struct Heap{s:u64,e:u64,c:u64}impl Heap{fn alloc(&mut self,l:u64)->*mut u8{let p=self.c;self.c+=l;p as *mut u8}}"
    submit_card(conn, "kernel_heap_allocator", f"echo '{heap_code}' > {KERNEL_ROOT}/kernel/mm/heap.anv")
    submit_card(conn, "setup_sched_dir", f"mkdir -p {KERNEL_ROOT}/kernel/sched")
    switch_asm = ".global switch_context;switch_context:;push %rbp;push %rbx;push %r12;push %r13;push %r14;push %r15;mov %rsp, (%rdi);mov (%rsi), %rsp;pop %r15;pop %r14;pop %r13;pop %r12;pop %rbx;pop %rbp;ret"
    submit_card(conn, "context_switch_asm", f"echo '{switch_asm}' > {KERNEL_ROOT}/kernel/sched/switch.S")
    task_code = "struct Task{id:u64,sp:u64,pt:u64}impl Task{fn new(id:u64)->Self{Task{id,sp:0,pt:0}}}"
    submit_card(conn, "process_struct", f"echo '{task_code}' > {KERNEL_ROOT}/kernel/sched/task.anv")
    pit_code = "struct Pit;impl Pit{fn init(){unsafe{asm!(\"outb %al,$0x43\",in(\"al\")0x36);asm!(\"outb %al,$0x40\",in(\"al\")0);asm!(\"outb %al,$0x40\",in(\"al\")0);}}}"
    submit_card(conn, "pit_scheduler", f"echo '{pit_code}' > {KERNEL_ROOT}/kernel/drivers/pit.anv")
    submit_card(conn, "setup_fs_dir", f"mkdir -p {KERNEL_ROOT}/kernel/fs")
    kb_code = "struct Kb;impl Kb{fn read()->u8{let s:u8;unsafe{asm!(\"inb $0x64,%al\",out(\"al\")s)};if s&1==1{let k:u8;unsafe{asm!(\"inb $0x60,%al\",out(\"al\")k)};k}else{0}}}"
    submit_card(conn, "ps2_keyboard_poller", f"echo '{kb_code}' > {KERNEL_ROOT}/kernel/drivers/keyboard.anv")
    vfs_code = "trait File{fn read(&self,b:&mut[u8])->usize;}trait Inode{fn open(&self)->&dyn File;}"
    submit_card(conn, "vfs_trait_definition", f"echo '{vfs_code}' > {KERNEL_ROOT}/kernel/fs/vfs.anv")
    initramfs_code = "struct Cpio;impl Cpio{fn parse(p:*const u8){}}"
    submit_card(conn, "initramfs_parser", f"echo '{initramfs_code}' > {KERNEL_ROOT}/kernel/fs/initramfs.anv")
    submit_card(conn, "setup_syscalls_dir", f"mkdir -p {KERNEL_ROOT}/kernel/syscalls {KERNEL_ROOT}/kernel/binfmt")
    sys_code = "fn syscall(n:u64,a:u64,b:u64)->u64{match n{1=>1,2=>2,_=>0}}"
    submit_card(conn, "syscall_dispatcher", f"echo '{sys_code}' > {KERNEL_ROOT}/kernel/syscalls/handler.anv")
    elf_code = "struct Elf{h:u64}impl Elf{fn load(&self){}}"
    submit_card(conn, "elf_loader", f"echo '{elf_code}' > {KERNEL_ROOT}/kernel/binfmt/elf.anv")
    man_code = "fn main(){print!(\"MAN PAGER\");}"
    submit_card(conn, "man_page_viewer", f"echo '{man_code}' > {KERNEL_ROOT}/tools/man_pager.anv")
    submit_card(conn, "strip_symbols", f"echo 'STRIP_DEBUG=1' >> {KERNEL_ROOT}/anvil.toml")
    ld_script = "ENTRY(_start)SECTIONS{.=0xffffffff80000000;.text:{*(.multiboot_header)*(.text)}.rodata:{*(.rodata)}.data:{*(.data)}.bss:{*(.bss)}}"
    submit_card(conn, "final_link_script", f"echo '{ld_script}' > {KERNEL_ROOT}/linker.ld")
    iso_sh = "mkdir -p iso/boot/grub;cp kernel.bin iso/boot/;echo \\\"menuentry \\\\\\\"Anvil\\\\\\\"{multiboot2 /boot/kernel.bin;boot}\\\" > iso/boot/grub/grub.cfg;grub-mkrescue -o anvil.iso iso"
    submit_card(conn, "generate_iso_recipe", f"echo '{iso_sh}' > {KERNEL_ROOT}/build_iso.sh")
    conn.commit()
    conn.close()
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
if __name__ == "__main__":
    main()
