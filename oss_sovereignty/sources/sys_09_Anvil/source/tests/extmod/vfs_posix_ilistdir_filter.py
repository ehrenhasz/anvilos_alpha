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
    dirs = [".a", "..a", "...a", "a.b", "a..b"]
    for dir in dirs:
        fs.mkdir(dir)
    dirs = []
    for entry in fs.ilistdir("/"):
        dirs.append(entry[0])
    dirs.sort()
    print(dirs)
    os.chdir(curdir)
temp_dir = "vfs_posix_ilistdir_filter_test_dir"
try:
    os.stat(temp_dir)
    print("SKIP")
    raise SystemExit
except OSError:
    pass
os.mkdir(temp_dir)
try:
    test(temp_dir)
finally:
    for td in os.listdir(temp_dir):
        os.rmdir("/".join((temp_dir, td)))
    os.rmdir(temp_dir)
