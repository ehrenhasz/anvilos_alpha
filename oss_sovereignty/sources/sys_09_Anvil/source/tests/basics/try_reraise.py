def f():
    try:
        raise ValueError("val", 3)
    except:
        raise
try:
    f()
except ValueError as e:
    print(repr(e))
try:
    raise
except RuntimeError:
    print("RuntimeError")
