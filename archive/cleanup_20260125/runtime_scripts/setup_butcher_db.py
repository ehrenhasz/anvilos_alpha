import sqlite3
import os
DB_PATH = os.path.join(os.path.dirname(__file__), "cortex.db")
def upgrade_db():
    print(f"Connecting to {DB_PATH}...")
    with sqlite3.connect(DB_PATH) as conn:
        cursor = conn.cursor()
        try:
            cursor.execute("ALTER TABLE library_index ADD COLUMN butchered_at DATETIME")
            print("Added 'butchered_at' column to library_index.")
        except sqlite3.OperationalError:
            print("'butchered_at' column already exists.")
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS fragments (
                id TEXT PRIMARY KEY,
                source_path TEXT,
                asset_type TEXT,
                name TEXT,
                content_json TEXT,
                raw_text TEXT,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        """)
        print("Ensured 'fragments' table exists.")
        cursor.execute("""
            CREATE VIEW IF NOT EXISTS view_butcher_queue AS
            SELECT rowid, * FROM library_index 
            WHERE butchered_at IS NULL 
            AND path LIKE '%.pdf'
        """)
        print("Ensured 'view_butcher_queue' exists.")
        conn.commit()
if __name__ == "__main__":
    upgrade_db()
