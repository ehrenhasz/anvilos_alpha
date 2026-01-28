#!/usr/bin/env python3
import os
import sys
IGNORE_DIRS = {
    'oss_sovereignty', 'vendor', '.venv', '.git', '__pycache__', 
    'build_artifacts', 'logs', 'data', 'config', 'DOCS', 'frontend', 'archive',
    'ext' # Don't touch toolchains
}
def strip_bloat(content):
    lines = content.splitlines()
    kept = []
    for i, line in enumerate(lines):
        if i == 0 and line.startswith("#!"):
            kept.append(line)
            continue
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("#"):
            continue
        kept.append(line)
    return "\n".join(kept) + "\n"
def process_file(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        print(f"Skipping binary/non-utf8 file: {filepath}")
        return
    new_content = strip_bloat(content)
    if new_content != content:
        print(f"Purging bloat from {filepath}")
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)
def main():
    root = os.getcwd()
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in IGNORE_DIRS]
        for name in filenames:
            if name.endswith('.py') or name.endswith('.sh') or name.endswith('.anv'):
                process_file(os.path.join(dirpath, name))
if __name__ == "__main__":
    main()
