try:
    from array import array
except ImportError:
    print("SKIP")
    raise SystemExit
print(array('i', (i for i in range(10))))
