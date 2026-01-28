try:
    import struct
except ImportError:
    print("SKIP")
    raise SystemExit
class A():
    pass
o = A()
s = struct.pack("<O", o)
o2 = struct.unpack("<O", s)
print(o is o2[0])
print(struct.pack('<2I', 1))
try:
    import uctypes
    o = uctypes.addressof('abc')
    s = struct.pack("<S", o)
    o2 = struct.unpack("<S", s)
    assert o2[0] == 'abc'
except ImportError:
    pass
