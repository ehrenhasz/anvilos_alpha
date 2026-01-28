try:
    from array import array
except ImportError:
    print("SKIP")
    raise SystemExit
print(bytearray(array('h', [1, 2])))
print(bytearray(array('I', [1, 2])))
