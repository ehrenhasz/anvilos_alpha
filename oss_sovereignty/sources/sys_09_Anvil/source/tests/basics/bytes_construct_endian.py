try:
    from array import array
except ImportError:
    print("SKIP")
    raise SystemExit
print(bytes(array('h', [1, 2])))
print(bytes(array('I', [1, 2])))
