import vfs
import machine, rp2
bdev = rp2.Flash()
try:
    fs = vfs.VfsFat(bdev)
except:
    vfs.VfsFat.mkfs(bdev)
    fs = vfs.VfsFat(bdev)
vfs.mount(fs, "/")
del vfs, bdev, fs
