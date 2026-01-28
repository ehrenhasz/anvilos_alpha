from machine import SPI
SPI_NUM = 2
spi = SPI(SPI_NUM, baudrate=5_000_000)
buf = bytearray(1024)
ok = True
for offs in range(0, len(buf)):
    v = memoryview(buf)[offs : offs + 128]
    spi.readinto(v, 0xFF)
    if not all(b == 0x00 for b in v):
        print(offs, v.hex())
        ok = False
print("Variable offset fixed length " + ("OK" if ok else "FAIL"))
if ok:
    for op_len in range(1, 66):
        wr = b"\xFF" * op_len
        for offs in range(1, len(buf) - op_len - 1):
            before = offs & 0xFF
            after = (~offs) & 0xFF
            buf[offs - 1] = before
            buf[offs + op_len] = after
            v = memoryview(buf)[offs : offs + op_len]
            spi.write_readinto(wr, v)
            if (
                not all(b == 0x00 for b in v)
                or buf[offs - 1] != before
                or buf[offs + op_len] != after
            ):
                print(v.hex())
                print(hex(op_len), hex(offs), hex(buf[offs - 1]), hex(buf[offs + op_len]))
                ok = False
    print("Variable offset and lengths " + ("OK" if ok else "FAIL"))
