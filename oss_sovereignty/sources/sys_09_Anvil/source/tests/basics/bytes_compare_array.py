try:
    import array
except ImportError:
    print("SKIP")
    raise SystemExit
print(array.array('b', [1, 2]) in b'\x01\x02\x03')
