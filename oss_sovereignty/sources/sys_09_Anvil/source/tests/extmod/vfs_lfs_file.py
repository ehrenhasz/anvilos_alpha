try:
    import vfs
    vfs.VfsLfs1
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
        if op == 4:  # block count
            return len(self.data) // self.ERASE_BLOCK_SIZE
        if op == 5:  # block size
            return self.ERASE_BLOCK_SIZE
        if op == 6:  # erase block
            return 0
def test(bdev, vfs_class):
    print("test", vfs_class)
    vfs_class.mkfs(bdev)
    fs = vfs_class(bdev)
    f = fs.open("test.txt", "wt")
    print(f)
    f.write("littlefs")
    f.close()
    f.close()
    f = fs.open("test.bin", "wb")
    print(f)
    f.write("littlefs")
    f.flush()
    f.close()
    f = fs.open("test.bin", "ab")
    f.write("more")
    f.close()
    f = fs.open("test2.bin", "xb")
    f.close()
    try:
        fs.open("test2.bin", "x")
    except OSError:
        print("open OSError")
    with fs.open("test.txt", "") as f:
        print(f.read())
    with fs.open("test.txt", "rt") as f:
        print(f.read())
    with fs.open("test.bin", "rb") as f:
        print(f.read())
    with fs.open("test.bin", "r+b") as f:
        print(f.read(8))
        f.write("MORE")
    with fs.open("test.bin", "rb") as f:
        print(f.read())
    f = fs.open("test.txt", "r")
    print(f.tell())
    f.seek(3, 0)
    print(f.tell())
    f.close()
    try:
        fs.open("noexist", "r")
    except OSError:
        print("open OSError")
    f1 = fs.open("test.txt", "")
    f2 = fs.open("test.bin", "b")
    print(f1.read())
    print(f2.read())
    f1.close()
    f2.close()
bdev = RAMBlockDevice(30)
test(bdev, vfs.VfsLfs1)
test(bdev, vfs.VfsLfs2)
