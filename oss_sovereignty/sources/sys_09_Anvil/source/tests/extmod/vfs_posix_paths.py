try:
    import os, vfs
    vfs.VfsPosix
except (ImportError, AttributeError):
    print("SKIP")
    raise SystemExit
temp_dir = "vfs_posix_paths_test_dir"
try:
    os.stat(temp_dir)
    print("SKIP")
    raise SystemExit
except OSError:
    pass
curdir = os.getcwd()
os.mkdir(temp_dir)
temp_dir_abs = os.getcwd() + os.sep + temp_dir
fs = vfs.VfsPosix(temp_dir_abs)
os.chdir(temp_dir_abs)
fs.mkdir("subdir")
fs.mkdir("subdir/one")
print('listdir("/"):', sorted(i[0] for i in fs.ilistdir("/")))
print('listdir("."):', sorted(i[0] for i in fs.ilistdir(".")))
print('getcwd() in {"", "/"}:', fs.getcwd() in {"", "/"})
print('chdir("subdir"):', fs.chdir("subdir"))
print("getcwd():", fs.getcwd())
print('mkdir("two"):', fs.mkdir("two"))
f = fs.open("file.py", "w")
f.write("print('hello')")
f.close()
print('listdir("/"):', sorted(i[0] for i in fs.ilistdir("/")))
print('listdir("/subdir"):', sorted(i[0] for i in fs.ilistdir("/subdir")))
print('listdir("."):', sorted(i[0] for i in fs.ilistdir(".")))
try:
    f = fs.open("/subdir/file.py", "r")
    print(f.read())
    f.close()
except Exception as e:
    print(e)
import sys
sys.path.insert(0, "")
try:
    import file
    print(file)
except Exception as e:
    print(e)
del sys.path[0]
fs.remove("file.py")
fs.rmdir("two")
fs.rmdir("/subdir/one")
fs.chdir("/")
fs.rmdir("/subdir")
os.chdir(curdir)
vfs.mount(vfs.VfsPosix(temp_dir_abs), "/mnt")
os.mkdir("/mnt/dir")
print('chdir("/mnt/dir"):', os.chdir("/mnt/dir"))
print("getcwd():", os.getcwd())
print('chdir("/mnt"):', os.chdir("/mnt"))
print("getcwd():", os.getcwd())
print('chdir("/"):', os.chdir("/"))
print("getcwd():", os.getcwd())
print('chdir("/mnt/dir"):', os.chdir("/mnt/dir"))
print("getcwd():", os.getcwd())
print('chdir(".."):', os.chdir(".."))
print("getcwd():", os.getcwd())
os.rmdir("/mnt/dir")
vfs.umount("/mnt")
os.chdir(curdir)
os.rmdir(temp_dir)
print(temp_dir in os.listdir())
