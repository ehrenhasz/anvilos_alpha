import sqlite3
import os
import sys

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DB_PATH = "/var/lib/anvilos/db/cortex.db"

def genesis():
    hostname = "anvil-node-01" # Simulated hostname
    print(f"[GENESIS] WAKING UP: {hostname}")
    
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    
    cur.execute("SELECT env_vars, boot_script FROM genesis_manifest WHERE hostname = ?", (hostname,))
    soul = cur.fetchone()
    
    if not soul:
        print(f"[GENESIS] No soul found for {hostname}. Provisioning default.")
        # Default soul: Run the agent.mpy
        default_script = f"MICROPYPATH={PROJECT_ROOT}/oss_sovereignty/sys_09_Anvil {PROJECT_ROOT}/oss_sovereignty/sys_09_Anvil/source/ports/unix/build-standard/micropython -c 'import agent'"
        cur.execute("INSERT INTO genesis_manifest (hostname, role, boot_script) VALUES (?, ?, ?)", (hostname, "runtime-worker", default_script))
        conn.commit()
        soul = ('{}', default_script)

    env_vars, script = soul
    print("[GENESIS] INJECTING VARIABLES...")
    # (Simplified for simulation)
    
    print("[GENESIS] MATERIALIZING PAYLOAD...")
    print(f"[GENESIS] EXECUTING: {script}")
    
    import subprocess
    subprocess.run(script, shell=True)

if __name__ == "__main__":
    genesis()
