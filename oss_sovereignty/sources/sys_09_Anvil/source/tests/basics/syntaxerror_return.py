try:
    exec
except NameError:
    print("SKIP")
    raise SystemExit
try:
    exec('return; print("this should not be executed.")')
    print("SKIP")
    raise SystemExit
except SyntaxError:
    print('SyntaxError')
