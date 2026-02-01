import re

pat_str_double = r"[^"]*(?:\\.[^"]*)*"
pat_str_single = r"'[^'\\]*(?:\\.[^'\\]*)*"
pat_cmnt_block = r'/\*[\s\S]*?\*/'
pat_cmnt_line = r'//[^\n]*'

# Combined
pattern = f'({pat_str_double}|{pat_str_single})|({pat_cmnt_block}|{pat_cmnt_line})'

print(f"Pattern: {pattern}")

regex = re.compile(pattern, re.MULTILINE)
print(f"Groups: {regex.groups}")

text = 'int main() { /* comment */ return 0; }'
match = regex.search(text)
if match:
    print(f"Match 1: {match.group(1)}")
    print(f"Match 2: {match.group(2)}")
else:
    print("No match")

