 

#include "py/runtime.h"

#if MICROPY_PY_VFS

#include "extmod/vfs.h"
#include "extmod/vfs_fat.h"
#include "extmod/vfs_lfs.h"
#include "extmod/vfs_posix.h"

#if !MICROPY_VFS
#error "MICROPY_PY_VFS requires MICROPY_VFS"
#endif

static const mp_rom_map_elem_t vfs_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_vfs) },

    { MP_ROM_QSTR(MP_QSTR_mount), MP_ROM_PTR(&mp_vfs_mount_obj) },
    { MP_ROM_QSTR(MP_QSTR_umount), MP_ROM_PTR(&mp_vfs_umount_obj) },
    #if MICROPY_VFS_FAT
    { MP_ROM_QSTR(MP_QSTR_VfsFat), MP_ROM_PTR(&mp_fat_vfs_type) },
    #endif
    #if MICROPY_VFS_LFS1
    { MP_ROM_QSTR(MP_QSTR_VfsLfs1), MP_ROM_PTR(&mp_type_vfs_lfs1) },
    #endif
    #if MICROPY_VFS_LFS2
    { MP_ROM_QSTR(MP_QSTR_VfsLfs2), MP_ROM_PTR(&mp_type_vfs_lfs2) },
    #endif
    #if MICROPY_VFS_POSIX
    { MP_ROM_QSTR(MP_QSTR_VfsPosix), MP_ROM_PTR(&mp_type_vfs_posix) },
    #endif
};
static MP_DEFINE_CONST_DICT(vfs_module_globals, vfs_module_globals_table);

const mp_obj_module_t mp_module_vfs = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&vfs_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_vfs, mp_module_vfs);

#endif 
