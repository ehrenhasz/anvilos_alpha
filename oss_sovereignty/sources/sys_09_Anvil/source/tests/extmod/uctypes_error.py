try:
    import uctypes
except ImportError:
    print("SKIP")
    raise SystemExit
data = bytearray(b"01234567")
try:
    uctypes.struct(data, {})
except TypeError:
    print("TypeError")
S = uctypes.struct(uctypes.addressof(data), {})
try:
    del S[0]
except TypeError:
    print("TypeError")
S = uctypes.struct(uctypes.addressof(data), [])
try:
    S.x
except TypeError:
    print("TypeError")
S = uctypes.struct(uctypes.addressof(data), {"x": []})
try:
    S.x
except TypeError:
    print("TypeError")
S = uctypes.struct(uctypes.addressof(data), {"x": (uctypes.ARRAY | 0, uctypes.INT8 | 2)})
try:
    S.x = 1
except TypeError:
    print("TypeError")
try:
    hash(S)
except TypeError:
    print("TypeError")
