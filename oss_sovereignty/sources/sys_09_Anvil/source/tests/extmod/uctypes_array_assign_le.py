try:
    import uctypes
except ImportError:
    print("SKIP")
    raise SystemExit
desc = {
    "arr": (uctypes.ARRAY | 0, uctypes.UINT8 | 2),
    "arr2": (uctypes.ARRAY | 0, 2, {"b": uctypes.UINT8 | 0}),
    "arr3": (uctypes.ARRAY | 2, uctypes.UINT16 | 2),
    "arr5": (uctypes.ARRAY | 0, uctypes.UINT32 | 1),
    "arr6": (uctypes.ARRAY | 1, uctypes.UINT32 | 1),
    "arr7": (uctypes.ARRAY | 0, 1, {"l": uctypes.UINT32 | 0}),
    "arr8": (uctypes.ARRAY | 1, 1, {"l": uctypes.UINT32 | 0}),
}
data = bytearray(6)
S = uctypes.struct(uctypes.addressof(data), desc, uctypes.LITTLE_ENDIAN)
S.arr[0] = 0x11
print(hex(S.arr[0]))
assert hex(S.arr[0]) == "0x11"
S.arr3[0] = 0x2233
print(hex(S.arr3[0]))
assert hex(S.arr3[0]) == "0x2233"
S.arr3[1] = 0x4455
print(hex(S.arr3[1]))
assert hex(S.arr3[1]) == "0x4455"
S.arr5[0] = 0x66778899
print(hex(S.arr5[0]))
assert hex(S.arr5[0]) == "0x66778899"
print(S.arr5[0] == S.arr7[0].l)
assert S.arr5[0] == S.arr7[0].l
S.arr6[0] = 0xAABBCCDD
print(hex(S.arr6[0]))
assert hex(S.arr6[0]) == "0xaabbccdd"
print(S.arr6[0] == S.arr8[0].l)
assert S.arr6[0] == S.arr8[0].l
