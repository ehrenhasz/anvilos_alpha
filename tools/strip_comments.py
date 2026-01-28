#!/usr/bin/env python3
import os
import re

TARGET_DIR = "oss_sovereignty"

# Regex patterns for removing comments
# C/C++/Java/etc block comments /* ... */
re_block = re.compile(r'/\*.*?\*/', re.DOTALL)
# C/C++ line comments // ...
re_line_c = re.compile(r'//.*')
# Script/Config line comments # ...
re_line_hash = re.compile(r'#.*')

def strip_file(filepath):
    try:
        # Detect encoding, defaulting to utf-8. Binary files will likely fail decoding.
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        # Simple heuristic: if it looks like a binary file (null bytes), skip it.
        if '\0' in content:
            return

        original_len = len(content)
        
        # Determine likely comment style based on extension
        ext = os.path.splitext(filepath)[1].lower()
        
        new_content = content
        
        # C/C++ style
        if ext in ['.c', '.h', '.cpp', '.hpp', '.cc', '.ld', '.s', '.S']:
             new_content = re_block.sub('', new_content)
             new_content = re_line_c.sub('', new_content)
        
        # Script/Config style
        elif ext in ['.py', '.sh', '.conf', '.cfg', '.yaml', '.yml', '.make', '.mk', '.ac', '.am', '.in'] or filepath.endswith('Makefile'):
             new_content = re_line_hash.sub('', new_content)
        
        # Generic fallback: try both if unsure, or skip if unknown.
        # But per "remove all comments from all files", we should be aggressive.
        # However, aggressively removing # in C files breaks preprocessor directives (#include).
        # So we must be careful with C-like files.
        
        if ext in ['.c', '.h', '.cpp', '.hpp', '.cc']:
            # For C/C++, we already did block and //
            # DO NOT run hash removal (preprocessor)
            pass
        else:
            # For others, assume hash is comment? 
            # This is risky for some formats, but user said "remove all comments".
            pass

        # Write back if changed
        if len(new_content) != original_len:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(new_content)
            # print(f"Stripped: {filepath}")

    except Exception as e:
        print(f"Error processing {filepath}: {e}")

def main():
    print(f"Stripping comments from files in {TARGET_DIR}...")
    count = 0
    for root, dirs, files in os.walk(TARGET_DIR):
        for name in files:
            filepath = os.path.join(root, name)
            strip_file(filepath)
            count += 1
            if count % 1000 == 0:
                print(f"Processed {count}...", end='\r')
    print(f"\nFinished processing {count} files.")

if __name__ == "__main__":
    main()
