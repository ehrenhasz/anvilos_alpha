import sqlite3
import os

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DB_PATH = "/var/lib/anvilos/db/cortex.db"

def init_cortex():
    print(f">> INITIALIZING CORTEX AT {DB_PATH}")
    conn = sqlite3.connect(DB_PATH)
    
    # Sector A: THE VOID (Unstructured Entropy)
    conn.execute("""
    CREATE TABLE IF NOT EXISTS void_logs (
        id TEXT PRIMARY KEY,
        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
        source_node TEXT,
        log_level TEXT,
        payload TEXT,
        metadata TEXT
    )""")

    # Sector B: THE FORGE (Crystallized Code)
    conn.execute("""
    CREATE TABLE IF NOT EXISTS forge_artifacts (
        id TEXT PRIMARY KEY,
        filepath TEXT UNIQUE NOT NULL,
        content TEXT NOT NULL,
        checksum TEXT NOT NULL,
        updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )""")

    # Sector C: GENESIS (Identity Manifest)
    conn.execute("""
    CREATE TABLE IF NOT EXISTS genesis_manifest (
        hostname TEXT PRIMARY KEY,
        role TEXT,
        active BOOLEAN DEFAULT 1,
        env_vars TEXT DEFAULT '{}',
        boot_script TEXT
    )""")

    # Sector D: ENGRAMS (Vector Memory)
    # Using simple text storage for now as sqlite doesn't natively have vector type
    conn.execute("""
    CREATE TABLE IF NOT EXISTS engrams (
        id TEXT PRIMARY KEY,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        content TEXT,
        embedding BLOB
    )""")

    # Existing jobs and agents tables (for Big Iron compatibility)
    conn.execute("""
    CREATE TABLE IF NOT EXISTS jobs (
        correlation_id TEXT PRIMARY KEY,
        idempotency_key TEXT UNIQUE,
        priority INTEGER DEFAULT 50,
        cost_center TEXT,
        payload TEXT,
        status TEXT DEFAULT 'PENDING',
        result TEXT,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )""")

    conn.execute("""
    CREATE TABLE IF NOT EXISTS agents (
        agent_id TEXT PRIMARY KEY,
        coding_id TEXT,
        status TEXT,
        updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )""")

    conn.commit()
    conn.close()
    print(">> CORTEX INITIALIZED.")

if __name__ == "__main__":
    init_cortex()
