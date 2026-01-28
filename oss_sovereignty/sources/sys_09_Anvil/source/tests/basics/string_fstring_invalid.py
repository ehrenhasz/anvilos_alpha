print(f"\\")
print(f"#")
try:
    eval("f'{\}'")
except SyntaxError:
    print("SyntaxError")
try:
    eval("f'{#}'")
except SyntaxError:
    print("SyntaxError")
