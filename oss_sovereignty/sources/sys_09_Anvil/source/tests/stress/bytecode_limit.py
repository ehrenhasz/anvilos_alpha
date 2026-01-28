body = " with f()()() as a:\n  try:\n   f()()()\n  except Exception:\n   pass\n"
for n in (433, 432, 431, 430):
    try:
        exec("cond = 0\nif cond:\n" + body * n + "else:\n print('cond false')\n")
    except MemoryError:
        print("SKIP")
        raise SystemExit
    except RuntimeError:
        print("RuntimeError")
exec(
    """
x = 0
if x:
"""
    + body * 13
    + """
x = [1 if x else 123]
print(x)
"""
)
