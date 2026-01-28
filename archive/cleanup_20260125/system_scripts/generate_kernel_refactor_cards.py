#!/usr/bin/env python3
import sqlite3
import json
import uuid
import os

# SUBMIT_CARDS = True
DB_PATH = "/var/lib/anvilos/db/cortex.db"

def generate_job(card_id, description, file_path, file_content):
    """
    Generates a job that, when executed as 'recipe.py', 
    writes the 'file_content' to 'file_path'.
    """
    
    # The recipe script that acts as the payload
    recipe_script = f"""
import os

target_path = "{file_path}"
os.makedirs(os.path.dirname(target_path), exist_ok=True)

content = {json.dumps(file_content)}

with open(target_path, "w") as f:
    f.write(content)

print(f"Successfully wrote {{len(content)}} bytes to {{target_path}}")
"""

    card_payload = {
        "description": description,
        "format": "recipe.py",
        "details": recipe_script
    }

    job = {
        "correlation_id": card_id,
        "idempotency_key": f"refactor-{card_id}",
        "priority": 80,
        "cost_center": "KERNEL_DEV",
        "payload": json.dumps(card_payload),
        "status": "PENDING"
    }
    return job

def main():
    print("--- Generating Kernel Refactor Cards (Phases 1 & 2) ---")
    
    jobs = []

    # --- PHASE 1 ---
    # 1.1 init/main.mpy
    content_1_1 = """# Anvil Kernel: init/main.mpy
import sys
try:
    from kernel import printk
except ImportError:
    # Placeholder for dev env
    class MockPrintk:
        def early_init_console(self):
            pass
        def printk(self, msg):
            print(f"[KERNEL] {msg}")
    printk = MockPrintk()

def start_kernel():
    printk.early_init_console()
    printk.printk("-- Anvil Kernel v0.1 --")
    printk.printk("Copyright (C) 2026, The Committee")
    printk.printk("Booting ANVIL-OS...")
    printk.printk("Performing critical initializations (stubs)...")
    
    # Initialize Memory (Phase 2)
    # from mm import bootmem
    # bootmem.init()

    printk.printk("HELLO SOVEREIGN")
    printk.printk("System Halted.")
    while True:
        pass

if __name__ == "__main__":
    start_kernel()
"""
    jobs.append(generate_job("anvil_kernel_1.1", "Refactor init/main.c to init/main.mpy", "init/main.mpy", content_1_1))

    # 1.2 kernel/printk.mpy
    content_1_2 = """# Anvil Kernel: kernel/printk.mpy
_console_drivers = []

def register_console(driver):
    if hasattr(driver, 'write'):
        _console_drivers.append(driver)

def printk(message):
    if not message.endswith('\n'): message += '\n'
    if not _console_drivers: return
    for d in _console_drivers:
        try:
            d.write(message)
        except:
            pass

class SerialConsole:
    def write(self, message):
        # TODO: UART Implementation
        print(f"[SERIAL] {message}", end='')

def early_init_console():
    register_console(SerialConsole())
"""
    jobs.append(generate_job("anvil_kernel_1.2", "Refactor kernel/printk/printk.c to kernel/printk.mpy", "kernel/printk.mpy", content_1_2))


    # --- PHASE 2 ---
    # 2.1 mm/bootmem.mpy
    content_2_1 = """# Anvil Kernel: mm/bootmem.mpy
# Simple boot-time memory allocator
# Tracks physical RAM bitmaps

_bitmap = []
_start_pfn = 0
_end_pfn = 0

def init(start, end):
    global _start_pfn, _end_pfn
    _start_pfn = start
    _end_pfn = end
    # Stub: Initialize bitmap
    print(f"[BOOTMEM] Initialized PFN {start}-{end}")

def alloc_bootmem(size):
    # Stub: Simple pointer bump
    print(f"[BOOTMEM] Allocating {size} bytes")
    return 0x100000 # Mock address
"""
    jobs.append(generate_job("anvil_kernel_2.1", "Refactor mm/bootmem.c to mm/bootmem.mpy", "mm/bootmem.mpy", content_2_1))

    # 2.2 mm/page_alloc.mpy
    content_2_2 = """# Anvil Kernel: mm/page_alloc.mpy
# Buddy Allocator

MAX_ORDER = 11
free_areas = [[] for _ in range(MAX_ORDER)]

def __init_zone(zone_start, zone_end):
    # Stub
    pass

def alloc_pages(order):
    # Stub
    print(f"[PAGE_ALLOC] Allocating 2^{order} pages")
    return None

def free_pages(addr, order):
    print(f"[PAGE_ALLOC] Freeing 2^{order} pages at {addr}")
"""
    jobs.append(generate_job("anvil_kernel_2.2", "Refactor mm/page_alloc.c to mm/page_alloc.mpy", "mm/page_alloc.mpy", content_2_2))

    # 2.3 mm/slab.mpy
    content_2_3 = """# Anvil Kernel: mm/slab.mpy
# Slab Allocator for object caching

caches = {}

class KmemCache:
    def __init__(self, name, size):
        self.name = name
        self.size = size
        self.slabs = []

def kmem_cache_create(name, size):
    if name in caches: return caches[name]
    c = KmemCache(name, size)
    caches[name] = c
    print(f"[SLAB] Created cache {name} (size {size})")
    return c

def kmem_cache_alloc(cache):
    # Stub
    return 0x200000

def kmalloc(size):
    # Find suitable cache or general cache
    print(f"[SLAB] kmalloc {size}")
    return 0x300000
"""
    jobs.append(generate_job("anvil_kernel_2.3", "Refactor mm/slab.c to mm/slab.mpy", "mm/slab.mpy", content_2_3))


    # SUBMIT TO DB
    conn = sqlite3.connect(DB_PATH)
    count = 0
    for job in jobs:
        try:
            conn.execute("""
                INSERT INTO jobs (correlation_id, idempotency_key, priority, cost_center, payload, status)
                VALUES (?, ?, ?, ?, ?, ?)
            """, (job['correlation_id'], job['idempotency_key'], job['priority'], job['cost_center'], job['payload'], job['status']))
            count += 1
            print(f"Queued: {job['correlation_id']}")
        except sqlite3.IntegrityError:
            print(f"Skipped (Duplicate): {job['correlation_id']}")
    
    conn.commit()
    conn.close()
    print(f"Submitted {count} jobs to Cortex.")

if __name__ == "__main__":
    main()
