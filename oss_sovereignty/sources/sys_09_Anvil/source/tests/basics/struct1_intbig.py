try:
    import struct
except ImportError:
    print("SKIP")
    raise SystemExit
print(struct.pack("<I", 2**32 - 1))
print(struct.pack("<I", 0xffffffff))
print(struct.pack("<Q", 1))
print(struct.pack(">Q", 1))
print(struct.pack("<Q", 2**64 - 1))
print(struct.pack(">Q", 2**64 - 1))
print(struct.pack("<Q", 0xffffffffffffffff))
print(struct.pack(">Q", 0xffffffffffffffff))
print(struct.pack("<q", -1))
print(struct.pack(">q", -1))
print(struct.pack("<Q", 1234567890123456789))
print(struct.pack("<q", -1234567890123456789))
print(struct.pack(">Q", 1234567890123456789))
print(struct.pack(">q", -1234567890123456789))
print(struct.unpack("<Q", b"\x12\x34\x56\x78\x90\x12\x34\x56"))
print(struct.unpack(">Q", b"\x12\x34\x56\x78\x90\x12\x34\x56"))
print(struct.unpack("<q", b"\x12\x34\x56\x78\x90\x12\x34\xf6"))
print(struct.unpack(">q", b"\xf2\x34\x56\x78\x90\x12\x34\x56"))
print(struct.unpack("<I", b"\xff\xff\xff\xff"))
print(struct.unpack("<Q", b"\xff\xff\xff\xff\xff\xff\xff\xff"))
print(struct.unpack("<i", b'\xff\xff\xff\x7f'))
print(struct.unpack("<q", b'\xff\xff\xff\xff\xff\xff\xff\x7f'))
bigzero = (1 << 70) - (1 << 70)
for endian in "<>":
    for type_ in "bhiq":
        fmt = endian + type_
        b = struct.pack(fmt, -2 + bigzero)
        print(fmt, b, struct.unpack(fmt, b))
