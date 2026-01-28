from micropython import const
import random, struct, machine, fwupdate, spiflash, pyb
_IOCTL_BLOCK_COUNT = const(4)
_IOCTL_BLOCK_SIZE = const(5)
_MBOOT_SPIFLASH_BASE_ADDR = 0x80000000
_RAW_FILESYSTEM_ADDR = const(4 * 1024)
_RAW_FILESYSTEM_LEN = const(1016 * 1024)
def _copy_file_to_raw_filesystem(filename, flash, block):
    block_count = flash.ioctl(_IOCTL_BLOCK_COUNT, 0)
    block_size = flash.ioctl(_IOCTL_BLOCK_SIZE, 0)
    buf = bytearray(block_size)
    with open(filename, "rb") as file:
        while True:
            n = file.readinto(buf)
            if not n:
                break
            flash.writeblocks(block, buf)
            block += 1
            if block >= block_count:
                print("|", end="")
                block = 0
            print(".", end="")
    print()
def update_app(filename):
    print(f"Updating application firmware from {filename}")
    flash = spiflash.SPIFlash(start=_RAW_FILESYSTEM_ADDR, len=_RAW_FILESYSTEM_LEN)
    block_count = flash.ioctl(_IOCTL_BLOCK_COUNT, 0)
    block_size = flash.ioctl(_IOCTL_BLOCK_SIZE, 0)
    block_start = random.randrange(0, block_count)
    print(f"Raw filesystem block layout: 0 .. {block_start} .. {block_count}")
    _copy_file_to_raw_filesystem(filename, flash, block_start)
    fwupdate.update_mpy(
        filename,
        fs_type=fwupdate.VFS_RAW,
        fs_base=_MBOOT_SPIFLASH_BASE_ADDR + _RAW_FILESYSTEM_ADDR + block_start * block_size,
        fs_len=(block_count - block_start) * block_size,
        fs_base2=_MBOOT_SPIFLASH_BASE_ADDR + _RAW_FILESYSTEM_ADDR,
        fs_len2=block_start * block_size,
    )
