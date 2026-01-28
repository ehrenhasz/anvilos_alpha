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
    
    print("--- PHASE 2: The Anvil Bootstrap (Card Generation) ---")
    
    # 1. init_anvil_manifest
    # Manifest in MicroJSON per mandate
    manifest_data = {
        "@ID": 1,
        "data": {
            "project": "Anvil-Kernel",
            "target": "x86_64-unknown-anvil-kernel",
            "version": "0.1.0-genesis"
        }
    }
    manifest_content = json.dumps(manifest_data, separators=(',', ':'))
    cmd_manifest = f"echo '{manifest_content}' > {KERNEL_ROOT}/anvil.toml"
    submit_card(conn, "init_anvil_manifest", cmd_manifest)
    
    # 2. transmute_entry_point
    # Header for Multiboot2. Minified, no comments.
    # Multiboot2 header requires: magic, architecture, header_length, checksum.
    # Plus a tag for the entry point.
    header_asm = (
        ".section .multiboot_header\n"
        "header_start:\n"
        ".long 0xe85250d6\n"
        ".long 0\n"
        ".long header_end - header_start\n"
        ".long 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start))\n"
        ".short 0\n"
        ".short 0\n"
        ".long 8\n"
        "header_end:\n"
        ".section .text\n"
        ".global _start\n"
        "_start:\n"
        "mov $0x2f4b2f4f,%eax\n" # 'OK' in VGA
        "mov %eax,0xb8000\n"
        "hlt"
    )
    # Minify ASM: replace newlines with semicolons or just join lines tightly
    minified_asm = header_asm.replace("\n", ";")
    cmd_entry = f"echo '{minified_asm}' > {KERNEL_ROOT}/arch/x86/boot/header.S"
    submit_card(conn, "transmute_entry_point", cmd_entry)

    conn.commit()
    conn.close()

if __name__ == "__main__":
    main()
