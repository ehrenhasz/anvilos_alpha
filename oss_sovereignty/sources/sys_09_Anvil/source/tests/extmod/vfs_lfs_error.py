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
        if op == 4:  
            return len(self.data) // self.ERASE_BLOCK_SIZE
        if op == 5:  
            return self.ERASE_BLOCK_SIZE
        if op == 6:  
            return 0
def test(bdev, vfs_class):
    print("test", vfs_class)
    try:
        vfs_class.mkfs(RAMBlockDevice(1))
    except OSError:
        print("mkfs OSError")
    try:
        vfs_class(bdev)
    except OSError:
        print("mount OSError")
    vfs_class.mkfs(bdev)
    fs = vfs_class(bdev)
    with fs.open("testfile", "w") as f:
        f.write("test")
    fs.mkdir("testdir")
    try:
        fs.ilistdir("noexist")
    except OSError:
        print("ilistdir OSError")
    try:
        fs.remove("noexist")
    except OSError:
        print("remove OSError")
    try:
        fs.rmdir("noexist")
    except OSError:
        print("rmdir OSError")
    try:
        fs.rename("noexist", "somethingelse")
    except OSError:
        print("rename OSError")
    try:
        fs.mkdir("testdir")
    except OSError:
        print("mkdir OSError")
    try:
        fs.chdir("noexist")
    except OSError:
        print("chdir OSError")
    print(fs.getcwd())  
    try:
        fs.chdir("testfile")
    except OSError:
        print("chdir OSError")
    print(fs.getcwd())  
    try:
        fs.stat("noexist")
    except OSError:
        print("stat OSError")
    with fs.open("testfile", "r") as f:
        f.seek(1 << 30)  
        try:
            f.seek(1 << 30, 1)  
        except OSError:
            print("seek OSError")
bdev = RAMBlockDevice(30)
test(bdev, vfs.VfsLfs1)
test(bdev, vfs.VfsLfs2)
