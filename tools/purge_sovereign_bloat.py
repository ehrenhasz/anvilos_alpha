#!/usr/bin/env python3
import os
import re
import sys
import multiprocessing

TARGET_DIRS = [
    'oss_sovereignty/linux-6.6.14',
    'oss_sovereignty/zfs',
    'oss_sovereignty/busybox'
]

# Regex for C/C++/Java/JS style comments (// and /* */)
# Captures strings (Group 1) to preserve them.
RE_C_COMMENTS = re.compile(
    r'(".*?(?<!\\)"|\".*?(?<!\\)")|(/\*.*?\*/|//[^\r\n]*$)',
    re.MULTILINE | re.DOTALL
)

# Regex for Hash style comments (# ...)
# Captures strings to preserve them.
RE_HASH_COMMENTS = re.compile(
    r'(".*?(?<!\\)"|\".*?(?<!\\)")|(#.*$)',
    re.MULTILINE
)

def strip_c_comments(text):
    def replacer(match):
        if match.group(1):
            return match.group(1) # Preserve string
        return " " # Replace comment with space to avoid merging tokens
    return RE_C_COMMENTS.sub(replacer, text)

def strip_hash_comments(text):
    def replacer(match):
        if match.group(1):
            return match.group(1)
        return ""
    return RE_HASH_COMMENTS.sub(replacer, text)

def clean_file(filepath):
    ext = os.path.splitext(filepath)[1].lower()
    
    try:
        # Use errors='replace' to avoid crashing on random binary files disguised as source
        with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()
    except Exception as e:
        return # Skip
    
    original_len = len(content)
    new_content = content

    # Select strategy based on extension
    if ext in ['.c', '.h', '.cpp', '.hpp', '.cc', '.ld', '.dts', '.dtsi']:
        new_content = strip_c_comments(new_content)
    elif ext in ['.py', '.sh', '.pl', '.pm', '.conf', '.config', '.mk', 'Makefile', 'Kconfig']:
        new_content = strip_hash_comments(new_content)
    else:
        # Default fallback for unknown text files? 
        # For safety, skipping unknown extensions to avoid corrupting data
        return

    # Remove blank lines
    lines = [line for line in new_content.splitlines() if line.strip()]
    new_content = "\n".join(lines) + "\n"

    if len(new_content) != original_len:
        try:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(new_content)
            # print(f"Cleaned {filepath}") 
        except Exception:
            pass

def process_directory(root_dir):
    print(f"Scanning {root_dir}...")
    files_to_process = []
    for root, dirs, files in os.walk(root_dir):
        if '.git' in dirs: dirs.remove('.git')
        for name in files:
            files_to_process.append(os.path.join(root, name))
    
    print(f"Found {len(files_to_process)} files in {root_dir}. processing...")
    
    # Use simple loop for safety/simplicity in this env
    count = 0
    for f in files_to_process:
        clean_file(f)
        count += 1
        if count % 1000 == 0:
            print(f"Processed {count} files...")

def main():
    print("Starting Sovereign Bloat Purge...")
    for d in TARGET_DIRS:
        if os.path.exists(d):
            process_directory(d)
        else:
            print(f"Skipping {d} (Not found)")
    print("Purge Complete.")

if __name__ == "__main__":
    main()
