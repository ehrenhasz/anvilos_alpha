try:
    import uctypes
except ImportError:
    print("SKIP")
    raise SystemExit
desc = {
    "arr": (uctypes.ARRAY | 0, uctypes.UINT8 | 2),
    "arr2": (uctypes.ARRAY | 2, uctypes.INT8 | 2),
}
data = bytearray(b"01234567")
S = uctypes.struct(uctypes.addressof(data), desc, uctypes.LITTLE_ENDIAN)
print(S.arr)
print(type(S.arr2))
print(bytearray(S))
