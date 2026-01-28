try:
    import os, vfs
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
def print_stat(st, print_size=True):
    print(st[:6], st[6] if print_size else -1, type(st[7]), type(st[8]), type(st[9]))
def test(bdev, vfs_class):
    print("test", vfs_class)
    vfs_class.mkfs(bdev)
    fs = vfs_class(bdev)
    print(fs.statvfs("/"))
    f = fs.open("test", "w")
    f.write("littlefs")
    f.close()
    print(fs.statvfs("/"))
    print(list(fs.ilistdir()))
    print(list(fs.ilistdir("/")))
    print(list(fs.ilistdir(b"/")))
    fs.mkdir("testdir")
    print(list(fs.ilistdir()))
    print(sorted(list(fs.ilistdir("testdir"))))
    fs.rmdir("testdir")
    print(list(fs.ilistdir()))
    fs.mkdir("testdir")
    print_stat(fs.stat("test"))
    print_stat(fs.stat("testdir"), False)
    with fs.open("test", "r") as f:
        print(f.read())
    with fs.open("testbig", "w") as f:
        data = "large012" * 32 * 16
        print("data length:", len(data))
        for i in range(4):
            print("write", i)
            f.write(data)
    print(fs.statvfs("/"))
    fs.rename("testbig", "testbig2")
    print(sorted(list(fs.ilistdir())))
    fs.chdir("testdir")
    fs.rename("/testbig2", "testbig2")
    print(sorted(list(fs.ilistdir())))
    fs.rename("testbig2", "/testbig2")
    fs.chdir("/")
    print(sorted(list(fs.ilistdir())))
    fs.remove("testbig2")
    print(sorted(list(fs.ilistdir())))
    fs.mkdir("/testdir2")
    fs.mkdir("/testdir/subdir")
    print(fs.getcwd())
    fs.chdir("/testdir")
    print(fs.getcwd())
    fs.open("test2", "w").close()
    print_stat(fs.stat("test2"))
    print_stat(fs.stat("/testdir/test2"))
    fs.remove("test2")
    fs.chdir("/")
    print(fs.getcwd())
    fs.chdir("testdir")
    print(fs.getcwd())
    fs.chdir("..")
    print(fs.getcwd())
    fs.chdir("testdir/subdir")
    print(fs.getcwd())
    fs.chdir("../..")
    print(fs.getcwd())
    fs.chdir("/./testdir2")
    print(fs.getcwd())
    fs.chdir("../testdir")
    print(fs.getcwd())
    fs.chdir("../..")
    print(fs.getcwd())
    fs.chdir(".//testdir")
    print(fs.getcwd())
    fs.chdir("subdir/./")
    print(fs.getcwd())
    fs.chdir("/")
    print(fs.getcwd())
    fs.rmdir("testdir/subdir")
    fs.rmdir("testdir")
    fs.rmdir("testdir2")
bdev = RAMBlockDevice(30)
test(bdev, vfs.VfsLfs1)
test(bdev, vfs.VfsLfs2)
