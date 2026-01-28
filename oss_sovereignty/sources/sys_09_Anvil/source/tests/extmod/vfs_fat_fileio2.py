try:
    import errno, os, vfs
    vfs.VfsFat
except (ImportError, AttributeError):
    print("SKIP")
    raise SystemExit
class RAMFS:
    SEC_SIZE = 512
    def __init__(self, blocks):
        self.data = bytearray(blocks * self.SEC_SIZE)
    def readblocks(self, n, buf):
        for i in range(len(buf)):
            buf[i] = self.data[n * self.SEC_SIZE + i]
    def writeblocks(self, n, buf):
        for i in range(len(buf)):
            self.data[n * self.SEC_SIZE + i] = buf[i]
    def ioctl(self, op, arg):
        if op == 4:  
            return len(self.data) // self.SEC_SIZE
        if op == 5:  
            return self.SEC_SIZE
try:
    bdev = RAMFS(50)
    vfs.VfsFat.mkfs(bdev)
except MemoryError:
    print("SKIP")
    raise SystemExit
fs = vfs.VfsFat(bdev)
vfs.mount(fs, "/ramdisk")
os.chdir("/ramdisk")
try:
    fs.mkdir("foo_dir")
except OSError as e:
    print(e.errno == errno.EEXIST)
try:
    fs.remove("foo_dir")
except OSError as e:
    print(e.errno == errno.EISDIR)
try:
    fs.remove("no_file.txt")
except OSError as e:
    print(e.errno == errno.ENOENT)
try:
    fs.rename("foo_dir", "/null/file")
except OSError as e:
    print(e.errno == errno.ENOENT)
with open("foo_dir/file-in-dir.txt", "w+t") as f:
    f.write("data in file")
with open("foo_dir/file-in-dir.txt", "r+b") as f:
    print(f.read())
with open("foo_dir/sub_file.txt", "w") as f:
    f.write("subdir file")
try:
    fs.rmdir("foo_dir")
except OSError as e:
    print(e.errno == errno.EACCES)
fs.rename("foo_dir/file-in-dir.txt", "foo_dir/file.txt")
print(list(fs.ilistdir("foo_dir")))
fs.rename("foo_dir/file.txt", "moved-to-root.txt")
print(list(fs.ilistdir()))
with open("temp", "w") as f:
    f.write("new text")
fs.rename("temp", "moved-to-root.txt")
print(list(fs.ilistdir()))
with open("moved-to-root.txt") as f:
    print(f.read())
fs.remove("foo_dir/sub_file.txt")
fs.rmdir("foo_dir")
print(list(fs.ilistdir()))
try:
    bsize = fs.statvfs("/ramdisk")[0]
    free = fs.statvfs("/ramdisk")[2] + 1
    f = open("large_file.txt", "wb")
    f.write(bytearray(bsize * free))
except OSError as e:
    print("ENOSPC:", e.errno == 28)  
f.close()
