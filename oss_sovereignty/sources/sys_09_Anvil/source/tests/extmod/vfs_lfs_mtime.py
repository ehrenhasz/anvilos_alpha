try:
    import time, vfs
    time.time
    time.sleep
    vfs.VfsLfs2
except (ImportError, AttributeError):
    print("SKIP")
    raise SystemExit
class RAMBlockDevice:
    ERASE_BLOCK_SIZE = 1024
    def __init__(self, blocks):
        self.data = bytearray(blocks * self.ERASE_BLOCK_SIZE)
    def readblocks(self, block, buf, off):
        addr = block * self.ERASE_BLOCK_SIZE + off
        for i in range(len(buf)):
            buf[i] = self.data[addr + i]
    def writeblocks(self, block, buf, off):
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
    print("test", vfs_class)
    vfs_class.mkfs(bdev)
    print("mtime=True")
    fs = vfs_class(bdev, mtime=True)
    current_time = int(time.time())
    fs.open("test1", "wt").close()
    time.sleep(1)
    fs.open("test2", "wt").close()
    stat1 = fs.stat("test1")
    stat2 = fs.stat("test2")
    print(stat1[8] != 0, stat2[8] != 0)
    print(current_time <= stat1[8] <= current_time + 1)
    print(stat1[8] < stat2[8])
    time.sleep(1)
    fs.open("test1", "rt").close()
    print(fs.stat("test1") == stat1)
    fs.open("test1", "wt").close()
    stat1_old = stat1
    stat1 = fs.stat("test1")
    print(stat1_old[8] < stat1[8])
    fs.umount()
    print("mtime=False")
    fs = vfs_class(bdev, mtime=False)
    print(fs.stat("test1") == stat1)
    print(fs.stat("test2") == stat2)
    f = fs.open("test1", "wt")
    f.close()
    print(fs.stat("test1") == stat1)
    fs.umount()
    print("mtime=True")
    fs = vfs_class(bdev, mtime=True)
    print(fs.stat("test1") == stat1)
    print(fs.stat("test2") == stat2)
    fs.umount()
try:
    bdev = RAMBlockDevice(30)
except MemoryError:
    print("SKIP")
    raise SystemExit
test(bdev, vfs.VfsLfs2)
