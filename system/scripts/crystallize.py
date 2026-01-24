import os
import hashlib
import sqlite3
import uuid

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DB_PATH = "/var/lib/anvilos/db/cortex.db"
KERNEL_ROOT = os.path.join(PROJECT_ROOT, "oss_sovereignty", "sys_01_Linux_Kernel", "source")

def crystallize():
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    
    extensions = ('.anv', '.S', '.ld', '.toml', '.sh')
    
    print(f">> CRYSTALLIZING KERNEL FROM {KERNEL_ROOT}")
    
    for root, _, files in os.walk(KERNEL_ROOT):
        for file in files:
            if file.endswith(extensions):
                full_path = os.path.join(root, file)
                if os.path.islink(full_path):
                    continue
                rel_path = os.path.relpath(full_path, PROJECT_ROOT)
                
                with open(full_path, 'r', errors='ignore') as f:
                    content = f.read()
                
                checksum = hashlib.sha256(content.encode()).hexdigest()
                
                cur.execute("""
                INSERT INTO forge_artifacts (id, filepath, content, checksum)
                VALUES (?, ?, ?, ?)
                ON CONFLICT (filepath) DO UPDATE
                SET content = EXCLUDED.content, checksum = EXCLUDED.checksum, updated_at = CURRENT_TIMESTAMP
                WHERE forge_artifacts.checksum != EXCLUDED.checksum
                """, (str(uuid.uuid4()), rel_path, content, checksum))
                
                print(f"Crystallized: {rel_path}")
                
    conn.commit()
    conn.close()
    print(">> CRYSTALLIZATION COMPLETE.")

if __name__ == "__main__":
    crystallize()
