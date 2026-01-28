try:
    import os, vfs
except ImportError:
    print("SKIP")
    raise SystemExit
class Filesystem:
    def __init__(self, id, fail=0):
        self.id = id
        self.fail = fail
    def mount(self, readonly, mkfs):
        print(self.id, "mount", readonly, mkfs)
    def umount(self):
        print(self.id, "umount")
    def ilistdir(self, dir):
        print(self.id, "ilistdir", dir)
        return iter([("a%d" % self.id, 0, 0)])
    def chdir(self, dir):
        print(self.id, "chdir", dir)
        if self.fail:
            raise OSError(self.fail)
    def getcwd(self):
        print(self.id, "getcwd")
        return "dir%d" % self.id
    def mkdir(self, path):
        print(self.id, "mkdir", path)
    def remove(self, path):
        print(self.id, "remove", path)
    def rename(self, old_path, new_path):
        print(self.id, "rename", old_path, new_path)
    def rmdir(self, path):
        print(self.id, "rmdir", path)
    def stat(self, path):
        print(self.id, "stat", path)
        return (self.id,)
    def statvfs(self, path):
        print(self.id, "statvfs", path)
        return (self.id,)
    def open(self, file, mode):
        print(self.id, "open", file, mode)
try:
    vfs.umount("/")
except OSError:
    pass
for path in os.listdir("/"):
    vfs.umount("/" + path)
print(os.stat("/"))
print(os.statvfs("/")[9] >= 32)
print(os.getcwd())
for func in ("chdir", "listdir", "mkdir", "remove", "rmdir", "stat"):
    for arg in ("x", "/x"):
        try:
            getattr(os, func)(arg)
        except OSError:
            print(func, arg, "OSError")
vfs.mount(Filesystem(1), "/test_mnt")
print(os.listdir())
i = os.ilistdir()
print(next(i))
try:
    next(i)
except StopIteration:
    print("StopIteration")
try:
    next(i)
except StopIteration:
    print("StopIteration")
print(os.listdir("test_mnt"))
print(os.listdir("/test_mnt"))
vfs.mount(Filesystem(2), "/test_mnt2", readonly=True)
print(os.listdir())
print(os.listdir("/test_mnt2"))
try:
    vfs.mount(Filesystem(3), "/test_mnt2")
except OSError:
    print("OSError")
try:
    os.mkdir("/test_mnt")
except OSError:
    print("OSError")
try:
    os.rename("/test_mnt/a", "/test_mnt2/b")
except OSError:
    print("OSError")
os.chdir("test_mnt")
print(os.listdir())
print(os.getcwd())
os.mkdir("test_dir")
os.remove("test_file")
os.rename("test_file", "test_file2")
os.rmdir("test_dir")
print(os.stat("test_file"))
print(os.statvfs("/test_mnt"))
open("test_file")
open("test_file", "wb")
vfs.umount("/test_mnt")
vfs.umount("/test_mnt2")
try:
    vfs.umount("/test_mnt")
except OSError:
    print("OSError")
vfs.mount(Filesystem(3), "/")
print(os.stat("/"))
print(os.statvfs("/"))
print(os.listdir())
open("test")
vfs.mount(Filesystem(4), "/mnt")
print(os.listdir())
print(os.listdir("/mnt"))
os.chdir("/mnt")
print(os.listdir())
os.chdir("/subdir")
print(os.listdir())
os.chdir("/")
vfs.umount("/")
print(os.listdir("/"))
vfs.umount("/mnt")
try:
    os.chdir("/foo")
except OSError:
    print("OSError")
print(os.getcwd())
vfs.mount(Filesystem(5, 1), "/mnt")
try:
    os.chdir("/mnt/subdir")
except OSError:
    print("OSError")
print(os.getcwd())
