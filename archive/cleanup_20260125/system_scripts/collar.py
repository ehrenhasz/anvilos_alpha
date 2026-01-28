import os
import sys
import re
import json # Added for JSON operations
class Collar:
    """
    RFC-030/RFC-028: The Collar
    Enforces the prohibition of unauthorized entropy sources.
    """
    BANNED_PATTERNS = [
        (r"import\s+random", "Standard 'random' library is BANNED. Use Hydrogen."),
        (r"from\s+random\s+import", "Standard 'random' library is BANNED. Use Hydrogen."),
        (r"/dev/urandom", "Direct access to /dev/urandom is BANNED. Use Hydrogen."),
        (r"~/\.gemini", "Usage of ~/.gemini is BANNED. Use /mnt/anvil_temp."),
        (r"console\.log", "Standard console.log is BANNED. Use strict JSON logging (MicroJSON).")
    ]
    IGNORE_DIRS = [
        ".git", "node_modules", "dist", "build", "__pycache__", "venv", "oss_sovereignty"
    ]
    IGNORE_SCAN_DIRS = [
        "oss_sovereignty"
    ]
    IGNORE_FILES = [
        "collar.py", "hydrogen.ts", "package-lock.json", "warden.py", "mainframe.js", "repl.js", "outline_engine.py" # Self-reference allowed for policing
    ]
    def check_active_card(self):
        db_path = "/var/lib/anvilos/db/cortex.db"
        if not os.path.exists(db_path):
            print(f"[COLLAR] Cortex DB not found at {db_path}")
            return False
        try:
            import sqlite3
            conn = sqlite3.connect(db_path)
            cursor = conn.cursor()
            cursor.execute("SELECT id, op, pld FROM card_stack WHERE stat IN (0, 1) LIMIT 1")
            row = cursor.fetchone()
            if row:
                print("=================================================")
                print(f"ACTIVE MAINFRAME CARD: {row[0]}")
                print("-------------------------------------------------")
                print(f"OP: {row[1]}")
                print("=================================================")
                conn.close()
                return True
            cursor.execute("SELECT correlation_id, payload FROM sys_jobs WHERE status IN ('PENDING', 'PROCESSING') LIMIT 1")
            row = cursor.fetchone()
            if row:
                print("=================================================")
                print(f"ACTIVE JOB: {row[0]}")
                print("-------------------------------------------------")
                print(f"PAYLOAD: {row[1][:100]}...")
                print("=================================================")
                conn.close()
                return True
            print("[COLLAR] No active cards or jobs found in Cortex.")
            conn.close()
            return False
        except Exception as e:
            print(f"[COLLAR] Error during DB check: {e}")
            sys.exit(1)
    def scan_file(self, filepath):
        violations = []
        try:
            with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                for pattern, message in self.BANNED_PATTERNS:
                    if re.search(pattern, content):
                        violations.append(message)
        except Exception as e:
            print(f"[COLLAR] Warning: Could not scan {filepath}: {e}")
        return violations
    def stage_files(self, root_dir):
        if not self.check_active_card():
            sys.exit(1)
        print(f"[COLLAR] Staging files in {root_dir}...")
        staged_count = 0
        skipped_count = 0
        IGNORE_EXTS = {'.o', '.obj', '.pyc', '.class', '.dll', '.exe', '.so', '.a', '.lib', '.iso', '.img', '.tar.gz', '.zip', '.swp'}
        LARGE_FILE_LIMIT = 10 * 1024 * 1024 
        files_to_stage = []
        for root, dirs, files in os.walk(root_dir):
            dirs[:] = [d for d in dirs if d not in self.IGNORE_DIRS and d not in ['tmp', 'temp', 'logs', 'artifacts', 'build', 'dist']]
            for file in files:
                filepath = os.path.join(root, file)
                _, ext = os.path.splitext(file)
                if ext.lower() in IGNORE_EXTS:
                    skipped_count += 1
                    continue
                try:
                    size = os.path.getsize(filepath)
                    if size > LARGE_FILE_LIMIT:
                        print(f"[SKIP] Large file ({size/1024/1024:.2f}MB): {filepath}")
                        skipped_count += 1
                        continue
                except OSError:
                    continue
                files_to_stage.append(filepath)
        BATCH_SIZE = 50
        for i in range(0, len(files_to_stage), BATCH_SIZE):
            batch = files_to_stage[i:i + BATCH_SIZE]
            try:
                subprocess.run(["git", "add"] + batch, check=True, capture_output=True)
                staged_count += len(batch)
                print(f"[COLLAR] Batch staged {len(batch)} files...")
            except subprocess.CalledProcessError as e:
                print(f"[ERROR] Failed to stage batch: {e}")
        print(f"[COLLAR] Staging complete. Staged: {staged_count}, Skipped: {skipped_count}")
    def check_git_hygiene(self, root_dir):
        """
        Enforces the Git Workflow Mandate:
        - No direct commits to main/master (unless initial).
        - Feature branches must follow 'feature/...' naming.
        """
        print(f"[COLLAR] Checking Git Hygiene in {root_dir}...")
        try:
            result = subprocess.run(["git", "branch", "--show-current"], cwd=root_dir, capture_output=True, text=True)
            current_branch = result.stdout.strip()
            if current_branch in ["main", "master"]:
                print("[COLLAR] WARNING: You are on the trunk branch. Ensure you checkout a feature branch before coding.")
            elif not current_branch.startswith("feature/"):
                print(f"[COLLAR] VIOLATION: Branch '{current_branch}' does not follow naming convention 'feature/TASK_ID'.")
                return False
            return True
        except Exception as e:
            print(f"[COLLAR] Git check failed: {e}")
            return False
    def scan_directory(self, root_dir, mode="cli"):
        if mode == "cli":
            print(f"[COLLAR] Scanning {root_dir} for entropy violations...")
        if not self.check_active_card():
            sys.exit(1)
        self.check_git_hygiene(root_dir)
        has_violations = False
        violations_data = []
        for root, dirs, files in os.walk(root_dir):
            dirs[:] = [d for d in dirs if d not in self.IGNORE_DIRS and d not in self.IGNORE_SCAN_DIRS]
            for file in files:
                if file in self.IGNORE_FILES:
                    continue
                if not file.endswith(('.py', '.js', '.ts', '.tsx', '.sh')):
                    continue
                filepath = os.path.join(root, file)
                violations = self.scan_file(filepath)
                if violations:
                    has_violations = True
                    violations_data.append({"path": filepath, "issues": violations})
                    if mode == "cli":
                        print(f"\n[VIOLATION] File: {filepath}")
                        for v in violations:
                            print(f"  -> {v}")
        if has_violations:
            if mode == "gui":
                print(f"GUI_MODE: FOUND {len(violations_data)} VIOLATIONS")
            elif mode == "cli":
                print("\n[COLLAR] VIOLATIONS DETECTED. ROLLBACK REQUIRED.")
            sys.exit(1)
        else:
            if mode == "gui":
                print("GUI_MODE: CLEAN")
            elif mode == "cli":
                print("\n[COLLAR] Clean. No unauthorized entropy sources detected.")
            sys.exit(0)
if __name__ == "__main__":
    import argparse
    import subprocess
    parser = argparse.ArgumentParser(description="Collar Entropy Scanner & Git Warden")
    parser.add_argument("command", nargs="?", default="read", choices=["read", "scan", "stage"], help="Action to perform: 'read' (default), 'scan', or 'stage'")
    parser.add_argument("root", nargs="?", default=".", help="Root directory to process for scanning")
    parser.add_argument("--go", choices=["cli", "gui"], default="cli", help="Execution mode for scanning")
    args = parser.parse_args()
    collar = Collar()
    if args.command == "read":
        collar.check_active_card()
    elif args.command == "scan":
        collar.scan_directory(args.root, mode=args.go)
    elif args.command == "stage":
        collar.stage_files(args.root)
