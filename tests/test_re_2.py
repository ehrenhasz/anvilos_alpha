import re
p = r'let\s+mut\s+(\w+)\s*:\s*([^=;]+);'
try:
    re.compile(p)
    print("Compiled")
except Exception as e:
    print(f"Error: {e}")
