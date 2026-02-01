#!/usr/bin/env python3
import os, re, sys

def strip_comments(text):
    Q, S, B = chr(34), chr(39), chr(92)
    p = "(" + Q + "(?:" + B + B + ".|[^" + Q + B + B + "])*" + Q + "|" + S + "(?:" + B + B + ".|[^" + S + B + B + "])*" + S + ")|/" + B + "*[\s\S]*?" + B + "*/|//[^\n]*"
    regex = re.compile(p)
    def _replacer(match):
        if match.group(1): return match.group(1)
        return " "
    return regex.sub(_replacer, text)

def main():
    if len(sys.argv) < 2: return
    target_dir = sys.argv[1]
    exts = {".c", ".h", ".cpp", ".hpp", ".cc", ".S", ".s"}
    count = 0
    for root, dirs, files in os.walk(target_dir):
        for f in files:
            if os.path.splitext(f)[1] in exts:
                path = os.path.join(root, f)
                try:
                    with open(path, "r", encoding="utf-8", errors="ignore") as fi:
                        content = fi.read()
                    new_content = strip_comments(content)
                    if content != new_content:
                        with open(path, "w", encoding="utf-8") as fo:
                            fo.write(new_content)
                        count += 1
                except: pass
    print("Purged comments from " + str(count) + " files.")

if __name__ == "__main__":
    main()