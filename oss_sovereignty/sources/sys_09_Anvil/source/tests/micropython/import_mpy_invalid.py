try:
    import sys, io, vfs
    sys.implementation._mpy
    io.IOBase
except (ImportError, AttributeError):
    print("SKIP")
    raise SystemExit
class UserFile(io.IOBase):
    def __init__(self, data):
        self.data = memoryview(data)
        self.pos = 0
    def readinto(self, buf):
        n = min(len(buf), len(self.data) - self.pos)
        buf[:n] = self.data[self.pos : self.pos + n]
        self.pos += n
        return n
    def ioctl(self, req, arg):
        return 0
class UserFS:
    def __init__(self, files):
        self.files = files
    def mount(self, readonly, mksfs):
        pass
    def umount(self):
        pass
    def stat(self, path):
        if path in self.files:
            return (32768, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        raise OSError
    def open(self, path, mode):
        return UserFile(self.files[path])
user_files = {
    "/mod0.mpy": b"",  
    "/mod1.mpy": b"M",  
    "/mod2.mpy": b"M\x00\x00\x00",  
}
vfs.mount(UserFS(user_files), "/userfs")
sys.path.append("/userfs")
for i in range(len(user_files)):
    mod = "mod%u" % i
    try:
        __import__(mod)
    except ValueError as er:
        print(mod, "ValueError", er)
vfs.umount("/userfs")
sys.path.pop()
