import gc
try:
    import os, vfs
    vfs.VfsPosix
except (ImportError, AttributeError):
    print("SKIP")
    raise SystemExit
def test(testdir):
    curdir = os.getcwd()
    fs = vfs.VfsPosix(testdir)
    os.chdir(testdir)
    fs.mkdir("/test_d1")
    fs.mkdir("/test_d2")
    fs.mkdir("/test_d3")
    for i in range(10):
        print(i)
        idir = fs.ilistdir("/")
        print(any(idir))
        for dname, *_ in fs.ilistdir("/"):
            fs.rmdir(dname)
            break
        fs.mkdir(dname)
        idir_emptied = fs.ilistdir("/")
        l = list(idir_emptied)
        print(len(l))
        try:
            next(idir_emptied)
        except StopIteration:
            pass
        gc.collect()
        fs.open("/test", "w").close()
        fs.remove("/test")
    os.chdir(curdir)
temp_dir = "vfs_posix_ilistdir_del_test_dir"
try:
    os.stat(temp_dir)
    print("SKIP")
    raise SystemExit
except OSError:
    pass
os.mkdir(temp_dir)
test(temp_dir)
for td in os.listdir(temp_dir):
    os.rmdir("/".join((temp_dir, td)))
os.rmdir(temp_dir)
