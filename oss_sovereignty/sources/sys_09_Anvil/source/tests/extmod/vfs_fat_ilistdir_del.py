import gc
try:
    import os, vfs
    vfs.VfsFat
except (ImportError, AttributeError):
    print("SKIP")
    raise SystemExit
class RAMBlockDevice:
    ERASE_BLOCK_SIZE = 512
    def __init__(self, blocks):
        self.data = bytearray(blocks * self.ERASE_BLOCK_SIZE)
    def readblocks(self, block, buf, off=0):
        addr = block * self.ERASE_BLOCK_SIZE + off
        for i in range(len(buf)):
            buf[i] = self.data[addr + i]
    def writeblocks(self, block, buf, off=0):
        addr = block * self.ERASE_BLOCK_SIZE + off
        for i in range(len(buf)):
            self.data[addr + i] = buf[i]
    def ioctl(self, op, arg):
        if op == 4:  
            return len(self.data) // self.ERASE_BLOCK_SIZE
        if op == 5:  
            return self.ERASE_BLOCK_SIZE
        if op == 6:  
            return 0
def test(bdev, vfs_class):
    vfs_class.mkfs(bdev)
    fs = vfs_class(bdev)
    fs.mkdir("/test_d1")
    fs.mkdir("/test_d2")
    fs.mkdir("/test_d3")
    for i in range(10):
        print(i)
        idir = fs.ilistdir("/")
        print(any(idir))
        for dname, *_ in fs.ilistdir("/"):
            fs.rmdir(dname)
            break
        fs.mkdir(dname)
        idir_emptied = fs.ilistdir("/")
        l = list(idir_emptied)
        print(len(l))
        try:
            next(idir_emptied)
        except StopIteration:
            pass
        gc.collect()
        fs.open("/test", "w").close()
try:
    bdev = RAMBlockDevice(50)
except MemoryError:
    print("SKIP")
    raise SystemExit
test(bdev, vfs.VfsFat)
