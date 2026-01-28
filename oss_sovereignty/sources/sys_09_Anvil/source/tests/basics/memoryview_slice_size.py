try:
    from sys import maxsize
    from uctypes import bytearray_at
    memoryview
except:
    print("SKIP")
    raise SystemExit
if maxsize <= 0xFFFF_FFFF:
    slice_max = 0xFF_FFFF
else:
    slice_max = 0xFF_FFFF_FFFF_FFFF
buf = bytearray_at(0, slice_max + 2)
mv = memoryview(buf)
print(mv[slice_max : slice_max + 1])
try:
    print(mv[slice_max + 1 : slice_max + 2])
except OverflowError:
    print("OverflowError")
