try:
    import io, os, vfs
    execfile
    io.IOBase
except (ImportError, NameError, AttributeError):
    print("SKIP")
    raise SystemExit
class File(io.IOBase):
    def __init__(self, data):
        self.data = data
        self.off = 0
    def ioctl(self, request, arg):
        return 0
    def readinto(self, buf):
        buf[:] = memoryview(self.data)[self.off : self.off + len(buf)]
        self.off += len(buf)
        return len(buf)
class Filesystem:
    def __init__(self, files):
        self.files = files
    def mount(self, readonly, mkfs):
        print("mount", readonly, mkfs)
    def umount(self):
        print("umount")
    def open(self, file, mode):
        print("open", file, mode)
        if file not in self.files:
            raise OSError(2)  
        return File(self.files[file])
try:
    vfs.umount("/")
except OSError:
    pass
for path in os.listdir("/"):
    vfs.umount("/" + path)
files = {
    "/test.py": "print(123)",
}
fs = Filesystem(files)
vfs.mount(fs, "/test_mnt")
try:
    execfile("/test_mnt/noexist.py")
except OSError:
    print("OSError")
execfile("/test_mnt/test.py")
try:
    execfile(b"aaa")
except TypeError:
    print("TypeError")
vfs.umount(fs)
