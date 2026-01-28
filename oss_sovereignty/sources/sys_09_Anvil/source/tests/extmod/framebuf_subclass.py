try:
    import framebuf, sys
except ImportError:
    print("SKIP")
    raise SystemExit
if sys.byteorder != "little":
    print("SKIP")
    raise SystemExit
class FB(framebuf.FrameBuffer):
    def __init__(self, n):
        self.n = n
        super().__init__(bytearray(2 * n * n), n, n, framebuf.RGB565)
    def foo(self):
        self.hline(0, 2, self.n, 0x0304)
fb = FB(n=3)
fb.pixel(0, 0, 0x0102)
fb.foo()
print(bytes(fb))
fb2 = framebuf.FrameBuffer(bytearray(2 * 3 * 3), 3, 3, framebuf.RGB565)
fb.fill(0)
fb.pixel(0, 0, 0x0506)
fb.pixel(2, 2, 0x0708)
fb2.blit(fb, 0, 0)
print(bytes(fb2))
class NotAFrameBuf:
    pass
try:
    fb.blit(NotAFrameBuf(), 0, 0)
except TypeError:
    print("TypeError")
try:
    fb.blit(None, 0, 0)
except TypeError:
    print("TypeError")
