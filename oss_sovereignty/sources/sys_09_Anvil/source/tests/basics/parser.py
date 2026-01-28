try:
    compile
except NameError:
    print("SKIP")
    raise SystemExit
try:
    compile("", "stdin", "eval")
except SyntaxError:
    print("SyntaxError")
compile("", "stdin", "exec")
try:
    compile("\\\n", "stdin", "single")
except SyntaxError:
    print("SyntaxError")
try:
    compile("\\\n", "stdin", "eval")
except SyntaxError:
    print("SyntaxError")
compile("\\\n", "stdin", "exec")
