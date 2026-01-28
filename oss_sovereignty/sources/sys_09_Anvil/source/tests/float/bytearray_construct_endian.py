try:
    from array import array
except ImportError:
    print("SKIP")
    raise SystemExit
print(bytearray(array("f", [1, 2.5])))
