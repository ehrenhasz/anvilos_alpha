try:
    from array import array
except ImportError:
    print("SKIP")
    raise SystemExit
print(bytes(array('b', [1, 2])))
print(bytes(array('h', [0x101, 0x202])))
