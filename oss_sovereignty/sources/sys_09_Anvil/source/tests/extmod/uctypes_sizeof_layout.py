try:
    import uctypes
except ImportError:
    print("SKIP")
    raise SystemExit
desc = {
    "f1": 0 | uctypes.UINT32,
    "f2": 4 | uctypes.UINT8,
}
print(uctypes.sizeof(desc) == uctypes.sizeof(desc, uctypes.NATIVE))
print(uctypes.sizeof(desc, uctypes.NATIVE) > uctypes.sizeof(desc, uctypes.LITTLE_ENDIAN))
s = uctypes.struct(0, desc, uctypes.LITTLE_ENDIAN)
try:
    uctypes.sizeof(s, uctypes.LITTLE_ENDIAN)
except TypeError:
    print("TypeError")
