try:
    import array
except ImportError:
    print("SKIP")
    raise SystemExit
print(b"123" + array.array('h', [0x1515]))
print(b"\x01\x02" + array.array('b', [1, 2]))
