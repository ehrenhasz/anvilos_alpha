import gc
import vfs
import samd
import sys
bdev = samd.Flash()
fs_type = vfs.VfsLfs2 if hasattr(vfs, "VfsLfs2") else vfs.VfsLfs1
try:
    fs = fs_type(bdev, progsize=256)
except:
    fs_type.mkfs(bdev, progsize=256)
    fs = fs_type(bdev, progsize=256)
vfs.mount(fs, "/")
sys.path.append("/lib")
del fs, fs_type, bdev, vfs, samd, sys
gc.collect()
del gc
