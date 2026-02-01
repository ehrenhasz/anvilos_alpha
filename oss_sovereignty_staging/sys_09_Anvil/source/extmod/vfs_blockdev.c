 

#include "py/runtime.h"
#include "py/binary.h"
#include "py/objarray.h"
#include "py/mperrno.h"
#include "extmod/vfs.h"

#if MICROPY_VFS

void mp_vfs_blockdev_init(mp_vfs_blockdev_t *self, mp_obj_t bdev) {
    mp_load_method(bdev, MP_QSTR_readblocks, self->readblocks);
    mp_load_method_maybe(bdev, MP_QSTR_writeblocks, self->writeblocks);
    mp_load_method_maybe(bdev, MP_QSTR_ioctl, self->u.ioctl);
    if (self->u.ioctl[0] != MP_OBJ_NULL) {
        
        self->flags |= MP_BLOCKDEV_FLAG_HAVE_IOCTL;
    } else {
        
        mp_load_method_maybe(bdev, MP_QSTR_sync, self->u.old.sync);
        mp_load_method(bdev, MP_QSTR_count, self->u.old.count);
    }
}

int mp_vfs_blockdev_read(mp_vfs_blockdev_t *self, size_t block_num, size_t num_blocks, uint8_t *buf) {
    if (self->flags & MP_BLOCKDEV_FLAG_NATIVE) {
        mp_uint_t (*f)(uint8_t *, uint32_t, uint32_t) = (void *)(uintptr_t)self->readblocks[2];
        return f(buf, block_num, num_blocks);
    } else {
        mp_obj_array_t ar = {{&mp_type_bytearray}, BYTEARRAY_TYPECODE, 0, num_blocks *self->block_size, buf};
        self->readblocks[2] = MP_OBJ_NEW_SMALL_INT(block_num);
        self->readblocks[3] = MP_OBJ_FROM_PTR(&ar);
        mp_call_method_n_kw(2, 0, self->readblocks);
        
        return 0;
    }
}

int mp_vfs_blockdev_read_ext(mp_vfs_blockdev_t *self, size_t block_num, size_t block_off, size_t len, uint8_t *buf) {
    mp_obj_array_t ar = {{&mp_type_bytearray}, BYTEARRAY_TYPECODE, 0, len, buf};
    self->readblocks[2] = MP_OBJ_NEW_SMALL_INT(block_num);
    self->readblocks[3] = MP_OBJ_FROM_PTR(&ar);
    self->readblocks[4] = MP_OBJ_NEW_SMALL_INT(block_off);
    mp_obj_t ret = mp_call_method_n_kw(3, 0, self->readblocks);
    if (ret == mp_const_none) {
        return 0;
    } else {
        return MP_OBJ_SMALL_INT_VALUE(ret);
    }
}

int mp_vfs_blockdev_write(mp_vfs_blockdev_t *self, size_t block_num, size_t num_blocks, const uint8_t *buf) {
    if (self->writeblocks[0] == MP_OBJ_NULL) {
        
        return -MP_EROFS;
    }

    if (self->flags & MP_BLOCKDEV_FLAG_NATIVE) {
        mp_uint_t (*f)(const uint8_t *, uint32_t, uint32_t) = (void *)(uintptr_t)self->writeblocks[2];
        return f(buf, block_num, num_blocks);
    } else {
        mp_obj_array_t ar = {{&mp_type_bytearray}, BYTEARRAY_TYPECODE, 0, num_blocks *self->block_size, (void *)buf};
        self->writeblocks[2] = MP_OBJ_NEW_SMALL_INT(block_num);
        self->writeblocks[3] = MP_OBJ_FROM_PTR(&ar);
        mp_call_method_n_kw(2, 0, self->writeblocks);
        
        return 0;
    }
}

int mp_vfs_blockdev_write_ext(mp_vfs_blockdev_t *self, size_t block_num, size_t block_off, size_t len, const uint8_t *buf) {
    if (self->writeblocks[0] == MP_OBJ_NULL) {
        
        return -MP_EROFS;
    }

    mp_obj_array_t ar = {{&mp_type_bytearray}, BYTEARRAY_TYPECODE, 0, len, (void *)buf};
    self->writeblocks[2] = MP_OBJ_NEW_SMALL_INT(block_num);
    self->writeblocks[3] = MP_OBJ_FROM_PTR(&ar);
    self->writeblocks[4] = MP_OBJ_NEW_SMALL_INT(block_off);
    mp_obj_t ret = mp_call_method_n_kw(3, 0, self->writeblocks);
    if (ret == mp_const_none) {
        return 0;
    } else {
        return MP_OBJ_SMALL_INT_VALUE(ret);
    }
}

mp_obj_t mp_vfs_blockdev_ioctl(mp_vfs_blockdev_t *self, uintptr_t cmd, uintptr_t arg) {
    if (self->flags & MP_BLOCKDEV_FLAG_HAVE_IOCTL) {
        
        self->u.ioctl[2] = MP_OBJ_NEW_SMALL_INT(cmd);
        self->u.ioctl[3] = MP_OBJ_NEW_SMALL_INT(arg);
        return mp_call_method_n_kw(2, 0, self->u.ioctl);
    } else {
        
        switch (cmd) {
            case MP_BLOCKDEV_IOCTL_SYNC:
                if (self->u.old.sync[0] != MP_OBJ_NULL) {
                    mp_call_method_n_kw(0, 0, self->u.old.sync);
                }
                break;

            case MP_BLOCKDEV_IOCTL_BLOCK_COUNT:
                return mp_call_method_n_kw(0, 0, self->u.old.count);

            case MP_BLOCKDEV_IOCTL_BLOCK_SIZE:
                
                break;

            case MP_BLOCKDEV_IOCTL_INIT:
                
                break;
        }
        return mp_const_none;
    }
}

#endif 
