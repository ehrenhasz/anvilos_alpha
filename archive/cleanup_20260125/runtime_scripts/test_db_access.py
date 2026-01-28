import sqlite3
import os
import sys
USER_HOME = os.path.expanduser("~")
CORTEX_DIR = os.path.join(USER_HOME, ".dnd_dm")
CORTEX_DB_PATH = "/var/lib/anvilos/db/cortex.db"
try:
    with sqlite3.connect(CORTEX_DB_PATH) as conn:
        cursor = conn.cursor()
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='library_index'")
        if cursor.fetchone():
            cursor.execute("SELECT COUNT(*) FROM library_index")
            count = cursor.fetchone()[0]
            print(f"Successfully accessed library_index table. Found {count} records.")
        else:
            print("library_index table does not exist. This is expected if the library is empty.")
    print("Database connection and basic query successful.")
except sqlite3.OperationalError as e:
    print(f"Database access failed (OperationalError): {e}")
    if "database is locked" in str(e):
        print("This might indicate a persistent lock. Ensure all processes accessing the DB are stopped.")
    sys.exit(1)
except Exception as e:
    print(f"An unexpected error occurred during database test: {e}")
    sys.exit(1)
