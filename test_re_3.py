import re
try:
    re.compile(r'(\w+):(?!:)')
    print("Compiled")
except Exception as e:
    print(f"Error: {e}")
