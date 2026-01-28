import sys, io, vfs
if not hasattr(io, "IOBase"):
    print("SKIP")
    raise SystemExit
"""
class A:
    def __init__(self, arg):
        self.arg = arg
    def write(self):
        pass
    def read(self):
        pass
def f():
    print, str, bytes, dict
    Exception, ValueError, TypeError
    x = "this will be a string object"
    x = b"this will be a bytes object"
    x = ("const tuple", None, False, True, 1, 2, 3)
result = 123
"""
file_data = b'M\x06\x00\x1f\x14\x03\x0etest.py\x00\x0f\x02A\x00\x02f\x00\x0cresult\x00/-5
class File(io.IOBase):
    def __init__(self):
        self.off = 0
    def ioctl(self, request, arg):
        return 0
    def readinto(self, buf):
        buf[:] = memoryview(file_data)[self.off : self.off + len(buf)]
        self.off += len(buf)
        return len(buf)
class FS:
    def mount(self, readonly, mkfs):
        pass
    def chdir(self, path):
        pass
    def stat(self, path):
        if path == "/__injected.mpy":
            return tuple(0 for _ in range(10))
        else:
            raise OSError(-2)  
    def open(self, path, mode):
        return File()
def mount():
    vfs.mount(FS(), "/__remote")
    sys.path.insert(0, "/__remote")
def test(r):
    global result
    for _ in r:
        sys.modules.clear()
        module = __import__("__injected")
    result = module.result
bm_params = {
    (32, 10): (50,),
    (1000, 10): (500,),
    (5000, 10): (5000,),
}
def bm_setup(params):
    (nloop,) = params
    mount()
    return lambda: test(range(nloop)), lambda: (nloop, result)
