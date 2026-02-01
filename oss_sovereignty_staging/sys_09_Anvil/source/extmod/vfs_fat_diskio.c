 

#include "py/mpconfig.h"
#if MICROPY_VFS && MICROPY_VFS_FAT

#include <stdint.h>
#include <stdio.h>

#include "py/mphal.h"

#include "py/runtime.h"
#include "py/binary.h"
#include "py/objarray.h"
#include "py/mperrno.h"
#include "lib/oofatfs/ff.h"
#include "lib/oofatfs/diskio.h"
#include "extmod/vfs_fat.h"

typedef void *bdev_t;
static fs_user_mount_t *disk_get_device(void *bdev) {
    return (fs_user_mount_t *)bdev;
}

 
 
 

DRESULT disk_read(
    bdev_t pdrv,       
    BYTE *buff,         
    DWORD sector,     
    UINT count         
    ) {
    fs_user_mount_t *vfs = disk_get_device(pdrv);
    if (vfs == NULL) {
        return RES_PARERR;
    }

    int ret = mp_vfs_blockdev_read(&vfs->blockdev, sector, count, buff);

    return ret == 0 ? RES_OK : RES_ERROR;
}

 
 
 

DRESULT disk_write(
    bdev_t pdrv,           
    const BYTE *buff,     
    DWORD sector,         
    UINT count             
    ) {
    fs_user_mount_t *vfs = disk_get_device(pdrv);
    if (vfs == NULL) {
        return RES_PARERR;
    }

    int ret = mp_vfs_blockdev_write(&vfs->blockdev, sector, count, buff);

    if (ret == -MP_EROFS) {
        
        return RES_WRPRT;
    }

    return ret == 0 ? RES_OK : RES_ERROR;
}


 
 
 

DRESULT disk_ioctl(
    bdev_t pdrv,       
    BYTE cmd,         
    void *buff         
    ) {
    fs_user_mount_t *vfs = disk_get_device(pdrv);
    if (vfs == NULL) {
        return RES_PARERR;
    }

    
    static const uint8_t op_map[8] = {
        [CTRL_SYNC] = MP_BLOCKDEV_IOCTL_SYNC,
        [GET_SECTOR_COUNT] = MP_BLOCKDEV_IOCTL_BLOCK_COUNT,
        [GET_SECTOR_SIZE] = MP_BLOCKDEV_IOCTL_BLOCK_SIZE,
        [IOCTL_INIT] = MP_BLOCKDEV_IOCTL_INIT,
    };
    uint8_t bp_op = op_map[cmd & 7];
    mp_obj_t ret = mp_const_none;
    if (bp_op != 0) {
        ret = mp_vfs_blockdev_ioctl(&vfs->blockdev, bp_op, 0);
    }

    
    switch (cmd) {
        case CTRL_SYNC:
            return RES_OK;

        case GET_SECTOR_COUNT: {
            *((DWORD *)buff) = mp_obj_get_int(ret);
            return RES_OK;
        }

        case GET_SECTOR_SIZE: {
            if (ret == mp_const_none) {
                
                *((WORD *)buff) = 512;
            } else {
                *((WORD *)buff) = mp_obj_get_int(ret);
            }
            
            vfs->blockdev.block_size = *((WORD *)buff);
            return RES_OK;
        }

        case GET_BLOCK_SIZE:
            *((DWORD *)buff) = 1; 
            return RES_OK;

        case IOCTL_INIT:
        case IOCTL_STATUS: {
            DSTATUS stat;
            if (ret != mp_const_none && MP_OBJ_SMALL_INT_VALUE(ret) != 0) {
                
                stat = STA_NOINIT;
            } else if (vfs->blockdev.writeblocks[0] == MP_OBJ_NULL) {
                stat = STA_PROTECT;
            } else {
                stat = 0;
            }
            *((DSTATUS *)buff) = stat;
            return RES_OK;
        }

        default:
            return RES_PARERR;
    }
}

#endif 
