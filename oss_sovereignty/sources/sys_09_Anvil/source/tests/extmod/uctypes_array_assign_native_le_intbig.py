import sys
try:
    import uctypes
except ImportError:
    print("SKIP")
    raise SystemExit
if sys.byteorder != "little":
    print("SKIP")
    raise SystemExit
desc = {
    "arr": (uctypes.ARRAY | 0, uctypes.UINT8 | 2),
    "arr2": (uctypes.ARRAY | 0, 2, {"b": uctypes.UINT8 | 0}),
    "arr3": (uctypes.ARRAY | 2, uctypes.UINT16 | 2),
    "arr5": (uctypes.ARRAY | 0, uctypes.UINT32 | 1),
    "arr7": (uctypes.ARRAY | 0, 1, {"l": uctypes.UINT32 | 0}),
    "arr8": (uctypes.ARRAY | 0, uctypes.INT8 | 1),
    "arr9": (uctypes.ARRAY | 0, uctypes.INT16 | 1),
    "arr10": (uctypes.ARRAY | 0, uctypes.INT32 | 1),
    "arr11": (uctypes.ARRAY | 0, uctypes.INT64 | 1),
    "arr12": (uctypes.ARRAY | 0, uctypes.UINT64 | 1),
    "arr13": (uctypes.ARRAY | 1, 1, {"l": {}}),
}
data = bytearray(8)
S = uctypes.struct(uctypes.addressof(data), desc)
S.arr11[0] = 0x11223344
print(hex(S.arr11[0]))
assert hex(S.arr11[0]) == "0x11223344"
S.arr12[0] = 0x11223344
print(hex(S.arr12[0]))
assert hex(S.arr12[0]) == "0x11223344"
