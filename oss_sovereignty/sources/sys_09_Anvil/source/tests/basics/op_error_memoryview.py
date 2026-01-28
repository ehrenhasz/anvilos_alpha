try:
    memoryview
except:
    print("SKIP")
    raise SystemExit
try:
    memoryview(b"") + b""
except TypeError:
    print("TypeError")
try:
    memoryview(b"") + memoryview(b"")
except TypeError:
    print("TypeError")
try:
    m = memoryview(bytearray())
    m += bytearray()
except TypeError:
    print("TypeError")
