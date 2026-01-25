#!/usr/bin/env python3
import sqlite3
import json

DB_PATH = "/var/lib/anvilos/db/cortex.db"

def poll_phase1_failures():
    """Queries the database for failed jobs related to Phase 1 and prints them."""
    try:
        conn = sqlite3.connect(DB_PATH)
        c = conn.cursor()
        c.execute("SELECT correlation_id, payload, result FROM jobs WHERE status='FAILED'")
        failed_jobs = c.fetchall()
        
        phase1_failures = []
        p1_contexts = [
            'purge_foreign', 
            'purge_localization', 
            'sanitize_documentation', 
            'strip_kconfig', 
            'remove_firmware'
        ]
        
        for job in failed_jobs:
            cid, payload, result = job
            try:
                p_dict = json.loads(payload)
                if any(ctx in p_dict.get('context', '') for ctx in p1_contexts):
                    phase1_failures.append({
                        'id': cid, 
                        'context': p_dict.get('context'), 
                        'result': result,
                        'details': p_dict.get('details')
                    })
            except (json.JSONDecodeError, AttributeError):
                continue
                
        if not phase1_failures:
            print('No Phase 1 cards found with FAILED status.')
        else:
            print("--- FAILED PHASE 1 CARDS ---")
            print(json.dumps(phase1_failures, indent=2))
            
    except sqlite3.Error as e:
        print(f"Database error: {e}")
    finally:
        if 'conn' in locals():
            conn.close()

if __name__ == "__main__":
    poll_phase1_failures()
