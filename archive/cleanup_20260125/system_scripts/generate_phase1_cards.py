import os
import sqlite3
import json
import uuid
DB_PATH = "/var/lib/anvilos/db/cortex.db"
KERNEL_ROOT = "oss_sovereignty/sys_01_Linux_Kernel/source"
ARCH_DIR = os.path.join(KERNEL_ROOT, "arch")
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
    existing = conn.execute("SELECT 1 FROM jobs WHERE payload LIKE ?", (f'%\"{context}\"%',)).fetchone()
    if existing:
        print(f"Skipping duplicate: {context}")
        return
    conn.execute("""
        INSERT INTO jobs (correlation_id, idempotency_key, priority, cost_center, payload, status)
        VALUES (?, ?, ?, ?, ?, 'PENDING')
    """, (correlation_id, f"op-{correlation_id}", 50, "OPS", json.dumps(card_payload)))
    print(f"Queued: {context}")
def main():
    conn = sqlite3.connect(DB_PATH)
    print("--- PHASE 1: The Great Purge (Card Generation) ---")
    if os.path.exists(ARCH_DIR):
        archs = [d for d in os.listdir(ARCH_DIR) if os.path.isdir(os.path.join(ARCH_DIR, d))]
        for arch in archs:
            if arch != "x86":
                cmd = f"rm -rf {os.path.join(ARCH_DIR, arch)}"
                submit_card(conn, f"purge_foreign_architectures_{arch}", cmd)
    cmd_files = f"find {KERNEL_ROOT} -type f \( -name '*.po' -o -name '*.mo' \) -delete"
    submit_card(conn, "purge_localization_files_extensions", cmd_files)
    cmd_dirs = f"find {KERNEL_ROOT} -type d -name 'locale' -exec rm -rf {{}} +"
    submit_card(conn, "purge_localization_files_dirs", cmd_dirs)
    doc_dir = os.path.join(KERNEL_ROOT, "Documentation")
    keep_dir = os.path.join(KERNEL_ROOT, "keep_man")
    submit_card(conn, "sanitize_documentation_setup", f"mkdir -p {keep_dir}")
    cmd_preserve = f"find {doc_dir} -type f -regex '.*\\.[0-9]$' -exec cp --parents {{}} {keep_dir} \;"
    submit_card(conn, "sanitize_documentation_preserve", cmd_preserve)
    submit_card(conn, "sanitize_documentation_wipe", f"rm -rf {doc_dir}/*")
    cmd_restore = f"if [ -d {keep_dir}/{doc_dir} ]; then cp -a {keep_dir}/{doc_dir}/* {doc_dir}/; fi; rm -rf {keep_dir}"
    submit_card(conn, "sanitize_documentation_restore", cmd_restore)
    submit_card(conn, "strip_kconfig_system", f"rm -rf {os.path.join(KERNEL_ROOT, 'scripts', 'kconfig')}")
    submit_card(conn, "remove_firmware_blobs", f"rm -rf {os.path.join(KERNEL_ROOT, 'firmware')}")
    conn.commit()
    conn.close()
if __name__ == "__main__":
    main()