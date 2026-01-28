import vfs
from flashbdev import bdev
def check_bootsec():
    buf = bytearray(bdev.ioctl(5, 0))  # 5 is SEC_SIZE
    bdev.readblocks(0, buf)
    empty = True
    for b in buf:
        if b != 0xFF:
            empty = False
            break
    if empty:
        return True
    fs_corrupted()
def fs_corrupted():
    import time
    import micropython
    micropython.kbd_intr(3)
    while 1:
        print(
            """\
The filesystem appears to be corrupted. If you had important data there, you
may want to make a flash snapshot to try to recover it. Otherwise, perform
factory reprogramming of MicroPython firmware (completely erase flash, followed
by firmware programming).
"""
        )
        time.sleep(3)
def setup():
    check_bootsec()
    print("Performing initial setup")
    if bdev.info()[4] == "vfs":
        vfs.VfsLfs2.mkfs(bdev)
        fs = vfs.VfsLfs2(bdev)
    elif bdev.info()[4] == "ffat":
        vfs.VfsFat.mkfs(bdev)
        fs = vfs.VfsFat(bdev)
    vfs.mount(fs, "/")
    with open("boot.py", "w") as f:
        f.write(
            """\
"""
        )
    return fs
