import vfs
import network
from flashbdev import bdev
def wifi():
    import binascii
    ap_if = network.WLAN(network.AP_IF)
    ssid = b"MicroPython-%s" % binascii.hexlify(ap_if.config("mac")[-3:])
    ap_if.config(ssid=ssid, security=network.AUTH_WPA_WPA2_PSK, key=b"micropythoN")
def check_bootsec():
    buf = bytearray(bdev.SEC_SIZE)
    bdev.readblocks(0, buf)
    empty = True
    for b in buf:
        if b != 0xFF:
            empty = False
            break
    if empty:
        return True
    fs_corrupted()
def fs_corrupted():
    import time
    import micropython
    micropython.kbd_intr(3)
    while 1:
        print(
            """\
The filesystem starting at sector %d with size %d sectors looks corrupt.
You may want to make a flash snapshot and try to recover it. Otherwise,
format it with vfs.VfsLfs2.mkfs(bdev), or completely erase the flash and
reprogram MicroPython.
"""
            % (bdev.start_sec, bdev.blocks)
        )
        time.sleep(3)
def setup():
    check_bootsec()
    print("Performing initial setup")
    wifi()
    vfs.VfsLfs2.mkfs(bdev)
    fs = vfs.VfsLfs2(bdev)
    vfs.mount(fs, "/")
    with open("boot.py", "w") as f:
        f.write(
            """\
import os, machine
import gc
gc.collect()
"""
        )
    return vfs
