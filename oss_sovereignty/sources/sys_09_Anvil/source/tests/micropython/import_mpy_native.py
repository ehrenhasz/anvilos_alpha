try:
    import sys, io, vfs
    sys.implementation._mpy
    io.IOBase
except (ImportError, AttributeError):
    print("SKIP")
    raise SystemExit
mpy_arch = sys.implementation._mpy >> 8
if mpy_arch >> 2 == 0:
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
valid_header = bytes([77, 6, mpy_arch, 31])
user_files = {
    '/mod0.mpy': bytes([77, 6, 0xfc | mpy_arch, 31]),
    '/mod1.mpy': valid_header + (
        b'\x02' 
        b'\x00' 
        b'\x0emod1.py\x00' 
        b'\x0aouter\x00' 
        b'\x2c' 
            b'\x00\x02' 
            b'\x01' 
            b'\x51' 
            b'\x63' 
        b'\x02' 
            b'\x42' 
                b'\x00\x00\x00\x00\x00\x00\x00\x00' 
                b'\x00' 
            b'\x43' 
                b'\x00\x00\x00\x00\x00\x00\x00\x00' 
                b'\x00\x00\x00' 
    ),
    '/mod2.mpy': valid_header + (
        b'\x02' 
        b'\x00' 
        b'\x0emod2.py\x00' 
        b'\x0aouter\x00' 
        b'\x2c' 
            b'\x00\x02' 
            b'\x01' 
            b'\x51' 
            b'\x63' 
        b'\x01' 
            b'\x22' 
                b'\x00\x00\x00\x00' 
                b'\x70' 
                b'\x06\x04' 
                b'rodata' 
                b'\x03\x01\x00' 
    ),
}
vfs.mount(UserFS(user_files), "/userfs")
sys.path.append("/userfs")
for i in range(len(user_files)):
    mod = "mod%u" % i
    try:
        __import__(mod)
        print(mod, "OK")
    except ValueError as er:
        print(mod, "ValueError", er)
vfs.umount("/userfs")
sys.path.pop()
