try:
    from array import array
except ImportError:
    print("SKIP")
    raise SystemExit
print(bytearray(array('b', [1, 2])))
print(bytearray(array('h', [0x101, 0x202])))
