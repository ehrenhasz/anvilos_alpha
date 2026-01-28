try:
    import os, vfs
    vfs.VfsFat
except (ImportError, AttributeError):
    print("SKIP")
    raise SystemExit
class RAMBDevSparse:
    SEC_SIZE = 512
    def __init__(self, blocks):
        self.blocks = blocks
        self.data = {}
    def readblocks(self, n, buf):
        assert len(buf) == self.SEC_SIZE
        if n not in self.data:
            self.data[n] = bytearray(self.SEC_SIZE)
        buf[:] = self.data[n]
    def writeblocks(self, n, buf):
        mv = memoryview(buf)
        for off in range(0, len(buf), self.SEC_SIZE):
            s = n + off // self.SEC_SIZE
            if s not in self.data:
                self.data[s] = bytearray(self.SEC_SIZE)
            self.data[s][:] = mv[off : off + self.SEC_SIZE]
    def ioctl(self, op, arg):
        if op == 4:  
            return self.blocks
        if op == 5:  
            return self.SEC_SIZE
try:
    bdev = RAMBDevSparse(4 * 1024 * 1024 * 1024 // RAMBDevSparse.SEC_SIZE)
    vfs.VfsFat.mkfs(bdev)
except MemoryError:
    print("SKIP")
    raise SystemExit
fs = vfs.VfsFat(bdev)
vfs.mount(fs, "/ramdisk")
print("statvfs:", fs.statvfs("/ramdisk"))
f = open("/ramdisk/test.txt", "w")
f.write("test file")
f.close()
print("statvfs:", fs.statvfs("/ramdisk"))
f = open("/ramdisk/test.txt")
print(f.read())
f.close()
vfs.umount(fs)
