try:
    import time, os, vfs
    time.time
    time.sleep
    vfs.VfsFat
except (ImportError, AttributeError):
    print("SKIP")
    raise SystemExit
class RAMBlockDevice:
    ERASE_BLOCK_SIZE = 512
    def __init__(self, blocks):
        self.data = bytearray(blocks * self.ERASE_BLOCK_SIZE)
    def readblocks(self, block, buf):
        addr = block * self.ERASE_BLOCK_SIZE
        for i in range(len(buf)):
            buf[i] = self.data[addr + i]
    def writeblocks(self, block, buf):
        addr = block * self.ERASE_BLOCK_SIZE
        for i in range(len(buf)):
            self.data[addr + i] = buf[i]
    def ioctl(self, op, arg):
        if op == 4:  
            return len(self.data) // self.ERASE_BLOCK_SIZE
        if op == 5:  
            return self.ERASE_BLOCK_SIZE
def test(bdev, vfs_class):
    print("test", vfs_class)
    vfs_class.mkfs(bdev)
    fs = vfs_class(bdev)
    current_time = int(time.time())
    fs.open("test1", "wt").close()
    time.sleep(2)
    fs.open("test2", "wt").close()
    stat1 = fs.stat("test1")
    stat2 = fs.stat("test2")
    print(stat1[8] != 0, stat2[8] != 0)
    print(stat1[8] < stat2[8])
    fs.umount()
bdev = RAMBlockDevice(50)
test(bdev, vfs.VfsFat)
