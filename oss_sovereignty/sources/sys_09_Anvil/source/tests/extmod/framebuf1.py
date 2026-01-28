try:
    import framebuf
except ImportError:
    print("SKIP")
    raise SystemExit
w = 5
h = 16
size = ((w + 7) & ~7) * ((h + 7) & ~7) // 8
buf = bytearray(size)
maps = {
    framebuf.MONO_VLSB: "MONO_VLSB",
    framebuf.MONO_HLSB: "MONO_HLSB",
    framebuf.MONO_HMSB: "MONO_HMSB",
}
for mapping in maps.keys():
    for x in range(size):
        buf[x] = 0
    fbuf = framebuf.FrameBuffer(buf, w, h, mapping)
    print(maps[mapping])
    print(memoryview(fbuf)[0])
    fbuf.fill(1)
    print(buf)
    fbuf.fill(0)
    print(buf)
    fbuf.pixel(0, 0, 1)
    fbuf.pixel(4, 0, 1)
    fbuf.pixel(0, 15, 1)
    fbuf.pixel(4, 15, 1)
    print(buf)
    fbuf.pixel(4, 15, 0)
    print(buf)
    print(fbuf.pixel(0, 0), fbuf.pixel(1, 1))
    fbuf.fill(0)
    fbuf.hline(0, 1, w, 1)
    print("hline", buf)
    fbuf.fill(0)
    fbuf.vline(1, 0, h, 1)
    print("vline", buf)
    fbuf.fill(0)
    fbuf.rect(1, 1, 3, 3, 1)
    print("rect", buf)
    fbuf.fill(0)
    fbuf.fill_rect(0, 0, 0, 3, 1)  # zero width, no-operation
    fbuf.fill_rect(1, 1, 3, 3, 1)
    print("fill_rect", buf)
    fbuf.fill(0)
    fbuf.line(1, 1, 3, 3, 1)
    print("line", buf)
    fbuf.fill(0)
    fbuf.line(3, 3, 2, 1, 1)
    print("line", buf)
    fbuf.fill(0)
    fbuf.pixel(2, 7, 1)
    fbuf.scroll(0, 1)
    print(buf)
    fbuf.scroll(0, -2)
    print(buf)
    fbuf.scroll(1, 0)
    print(buf)
    fbuf.scroll(-1, 0)
    print(buf)
    fbuf.scroll(2, 2)
    print(buf)
    fbuf.fill(0)
    fbuf.text("hello", 0, 0, 1)
    print(buf)
    fbuf.text("hello", 0, 0, 0)  # clear
    print(buf)
    fbuf.text(str(chr(31)), 0, 0)
    print(buf)
    print()
try:
    fbuf = framebuf.FrameBuffer(buf, w, h, -1, w)
except ValueError:
    print("ValueError")
if hasattr(framebuf, "FrameBuffer1"):
    fbuf = framebuf.FrameBuffer1(buf, w, h)
    fbuf = framebuf.FrameBuffer1(buf, w, h, w)
print(framebuf.MVLSB == framebuf.MONO_VLSB)
fbuf = framebuf.FrameBuffer(bytearray(2), 8, 1, framebuf.MONO_HLSB)
fbuf.pixel(0, 0, 1)
fbuf.pixel(4, 0, 1)
print(bytearray(fbuf))
