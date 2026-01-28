try:
    exec
except NameError:
    print("SKIP")
    raise SystemExit
args = {'thisisaverylongargumentname': 123}
exec("def foo(*,thisisaverylongargumentname=1):\n print(thisisaverylongargumentname)")
foo()
foo(**args)
