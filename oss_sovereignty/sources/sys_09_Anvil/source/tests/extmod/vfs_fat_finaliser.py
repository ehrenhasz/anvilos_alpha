try:
    import errno, os, vfs
    vfs.VfsFat
except (ImportError, AttributeError):
    print("SKIP")
    raise SystemExit
class RAMBlockDevice:
    def __init__(self, blocks, sec_size=512):
        self.sec_size = sec_size
        self.data = bytearray(blocks * self.sec_size)
    def readblocks(self, n, buf):
        for i in range(len(buf)):
            buf[i] = self.data[n * self.sec_size + i]
    def writeblocks(self, n, buf):
        for i in range(len(buf)):
            self.data[n * self.sec_size + i] = buf[i]
    def ioctl(self, op, arg):
        if op == 4:  
            return len(self.data) // self.sec_size
        if op == 5:  
            return self.sec_size
try:
    import errno, os
    bdev = RAMBlockDevice(50)
except MemoryError:
    print("SKIP")
    raise SystemExit
vfs.VfsFat.mkfs(bdev)
fs = vfs.VfsFat(bdev)
import micropython
micropython.heap_lock()
try:
    import errno, os
    fs.open("x", "r")
except MemoryError:
    print("MemoryError")
micropython.heap_unlock()
import gc
f = None
n = None
names = ["x%d" % i for i in range(5)]
for i in range(1024):
    []
for n in names:
    f = fs.open(n, "w")
    f.write(n)
    f = None  
gc.collect()  
for n in names[:-1]:
    with fs.open(n, "r") as f:
        print(f.read())
