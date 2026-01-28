import esp
class FlashBdev:
    SEC_SIZE = 4096
    def __init__(self, start_sec, blocks):
        self.start_sec = start_sec
        self.blocks = blocks
    def readblocks(self, n, buf, off=0):
        esp.flash_read((n + self.start_sec) * self.SEC_SIZE + off, buf)
    def writeblocks(self, n, buf, off=None):
        if off is None:
            esp.flash_erase(n + self.start_sec)
            off = 0
        esp.flash_write((n + self.start_sec) * self.SEC_SIZE + off, buf)
    def ioctl(self, op, arg):
        if op == 4:  # MP_BLOCKDEV_IOCTL_BLOCK_COUNT
            return self.blocks
        if op == 5:  # MP_BLOCKDEV_IOCTL_BLOCK_SIZE
            return self.SEC_SIZE
        if op == 6:  # MP_BLOCKDEV_IOCTL_BLOCK_ERASE
            esp.flash_erase(arg + self.start_sec)
            return 0
size = esp.flash_size()
if size < 1024 * 1024:
    bdev = None
else:
    start_sec = esp.flash_user_start() // FlashBdev.SEC_SIZE
    if start_sec < 256:
        start_sec += 1  # Reserve space for native code
    bdev = FlashBdev(start_sec, (size - 20480) // FlashBdev.SEC_SIZE - start_sec)
