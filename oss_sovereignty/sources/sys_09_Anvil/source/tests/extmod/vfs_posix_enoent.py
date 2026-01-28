try:
    import os, sys, vfs
    vfs.VfsPosix
except (ImportError, AttributeError):
    print("SKIP")
    raise SystemExit
if sys.platform == "win32":
    print("SKIP")
    raise SystemExit
temp_dir = "vfs_posix_enoent_test_dir"
try:
    os.stat(temp_dir)
    print("SKIP")
    raise SystemExit
except OSError:
    pass
curdir = os.getcwd()
os.mkdir(temp_dir)
os.chdir(temp_dir)
os.rmdir(curdir + "/" + temp_dir)
try:
    print("getcwd():", os.getcwd())
except OSError as e:
    print("getcwd():", repr(e))
try:
    print("VfsPosix():", vfs.VfsPosix("something"))
except OSError as e:
    print("VfsPosix():", repr(e))
