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
    def readblocks(self, block, buf, off=0):
        addr = block * self.ERASE_BLOCK_SIZE + off
        for i in range(len(buf)):
            buf[i] = self.data[addr + i]
    def writeblocks(self, block, buf, off=0):
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
def test(vfs_class):
    print("test", vfs_class)
    bdev = RAMBlockDevice(30)
    try:
        vfs.mount(bdev, "/lfs")
    except Exception as er:
        print(repr(er))
    vfs_class.mkfs(bdev)
    fs = vfs_class(bdev)
    vfs.mount(fs, "/lfs")
    with open("/lfs/lfsmod.py", "w") as f:
        f.write('print("hello from lfs")\n')
    import lfsmod
    os.mkdir("/lfs/lfspkg")
    with open("/lfs/lfspkg/__init__.py", "w") as f:
        f.write('print("package")\n')
    import lfspkg
    os.mkdir("/lfs/subdir")
    os.chdir("/lfs/subdir")
    os.rename("/lfs/lfsmod.py", "/lfs/subdir/lfsmod2.py")
    import lfsmod2
    vfs.umount("/lfs")
    fs = vfs_class(bdev)
    vfs.mount(fs, "/lfs", readonly=True)
    with open("/lfs/subdir/lfsmod2.py") as f:
        print("lfsmod2.py:", f.read())
    try:
        open("/lfs/test_write", "w")
    except OSError as er:
        print(repr(er))
    vfs.umount("/lfs")
    vfs.mount(bdev, "/lfs")
    vfs.umount("/lfs")
    sys.modules.clear()
import sys
sys.path.clear()
sys.path.append("/lfs")
sys.path.append("")
test(vfs.VfsLfs1)
test(vfs.VfsLfs2)
