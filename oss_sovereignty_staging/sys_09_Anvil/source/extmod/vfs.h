 
#ifndef MICROPY_INCLUDED_EXTMOD_VFS_H
#define MICROPY_INCLUDED_EXTMOD_VFS_H

#include "py/builtin.h"
#include "py/obj.h"



#define MP_VFS_NONE ((mp_vfs_mount_t *)1)
#define MP_VFS_ROOT ((mp_vfs_mount_t *)0)


#define MP_S_IFDIR (0x4000)
#define MP_S_IFREG (0x8000)


#define MP_BLOCKDEV_FLAG_NATIVE         (0x0001) 
#define MP_BLOCKDEV_FLAG_FREE_OBJ       (0x0002) 
#define MP_BLOCKDEV_FLAG_HAVE_IOCTL     (0x0004) 
#define MP_BLOCKDEV_FLAG_NO_FILESYSTEM  (0x0008) 


#define MP_BLOCKDEV_IOCTL_INIT          (1)
#define MP_BLOCKDEV_IOCTL_DEINIT        (2)
#define MP_BLOCKDEV_IOCTL_SYNC          (3)
#define MP_BLOCKDEV_IOCTL_BLOCK_COUNT   (4)
#define MP_BLOCKDEV_IOCTL_BLOCK_SIZE    (5)
#define MP_BLOCKDEV_IOCTL_BLOCK_ERASE   (6)


typedef struct _mp_vfs_proto_t {
    mp_import_stat_t (*import_stat)(void *self, const char *path);
} mp_vfs_proto_t;

typedef struct _mp_vfs_blockdev_t {
    uint16_t flags;
    size_t block_size;
    mp_obj_t readblocks[5];
    mp_obj_t writeblocks[5];
    
    union {
        mp_obj_t ioctl[4];
        struct {
            mp_obj_t sync[2];
            mp_obj_t count[2];
        } old;
    } u;
} mp_vfs_blockdev_t;

typedef struct _mp_vfs_mount_t {
    const char *str; 
    size_t len;
    mp_obj_t obj;
    struct _mp_vfs_mount_t *next;
} mp_vfs_mount_t;

void mp_vfs_blockdev_init(mp_vfs_blockdev_t *self, mp_obj_t bdev);
int mp_vfs_blockdev_read(mp_vfs_blockdev_t *self, size_t block_num, size_t num_blocks, uint8_t *buf);
int mp_vfs_blockdev_read_ext(mp_vfs_blockdev_t *self, size_t block_num, size_t block_off, size_t len, uint8_t *buf);
int mp_vfs_blockdev_write(mp_vfs_blockdev_t *self, size_t block_num, size_t num_blocks, const uint8_t *buf);
int mp_vfs_blockdev_write_ext(mp_vfs_blockdev_t *self, size_t block_num, size_t block_off, size_t len, const uint8_t *buf);
mp_obj_t mp_vfs_blockdev_ioctl(mp_vfs_blockdev_t *self, uintptr_t cmd, uintptr_t arg);

mp_vfs_mount_t *mp_vfs_lookup_path(const char *path, const char **path_out);
mp_import_stat_t mp_vfs_import_stat(const char *path);
mp_obj_t mp_vfs_mount(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
mp_obj_t mp_vfs_umount(mp_obj_t mnt_in);
mp_obj_t mp_vfs_open(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
mp_obj_t mp_vfs_chdir(mp_obj_t path_in);
mp_obj_t mp_vfs_getcwd(void);
mp_obj_t mp_vfs_ilistdir(size_t n_args, const mp_obj_t *args);
mp_obj_t mp_vfs_listdir(size_t n_args, const mp_obj_t *args);
mp_obj_t mp_vfs_mkdir(mp_obj_t path_in);
mp_obj_t mp_vfs_remove(mp_obj_t path_in);
mp_obj_t mp_vfs_rename(mp_obj_t old_path_in, mp_obj_t new_path_in);
mp_obj_t mp_vfs_rmdir(mp_obj_t path_in);
mp_obj_t mp_vfs_stat(mp_obj_t path_in);
mp_obj_t mp_vfs_statvfs(mp_obj_t path_in);

int mp_vfs_mount_and_chdir_protected(mp_obj_t bdev, mp_obj_t mount_point);

MP_DECLARE_CONST_FUN_OBJ_KW(mp_vfs_mount_obj);
MP_DECLARE_CONST_FUN_OBJ_1(mp_vfs_umount_obj);
MP_DECLARE_CONST_FUN_OBJ_KW(mp_vfs_open_obj);
MP_DECLARE_CONST_FUN_OBJ_1(mp_vfs_chdir_obj);
MP_DECLARE_CONST_FUN_OBJ_0(mp_vfs_getcwd_obj);
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(mp_vfs_ilistdir_obj);
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(mp_vfs_listdir_obj);
MP_DECLARE_CONST_FUN_OBJ_1(mp_vfs_mkdir_obj);
MP_DECLARE_CONST_FUN_OBJ_1(mp_vfs_remove_obj);
MP_DECLARE_CONST_FUN_OBJ_2(mp_vfs_rename_obj);
MP_DECLARE_CONST_FUN_OBJ_1(mp_vfs_rmdir_obj);
MP_DECLARE_CONST_FUN_OBJ_1(mp_vfs_stat_obj);
MP_DECLARE_CONST_FUN_OBJ_1(mp_vfs_statvfs_obj);

#endif 
