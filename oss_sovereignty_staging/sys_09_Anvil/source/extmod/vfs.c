 

#include <stdint.h>
#include <string.h>

#include "py/runtime.h"
#include "py/objstr.h"
#include "py/mperrno.h"
#include "extmod/vfs.h"

#if MICROPY_VFS

#if MICROPY_VFS_FAT
#include "extmod/vfs_fat.h"
#endif

#if MICROPY_VFS_LFS1 || MICROPY_VFS_LFS2
#include "extmod/vfs_lfs.h"
#endif

#if MICROPY_VFS_POSIX
#include "extmod/vfs_posix.h"
#endif



#define PROXY_MAX_ARGS (2)





mp_vfs_mount_t *mp_vfs_lookup_path(const char *path, const char **path_out) {
    if (*path == '/' || MP_STATE_VM(vfs_cur) == MP_VFS_ROOT) {
        
        bool is_abs = 0;
        if (*path == '/') {
            ++path;
            is_abs = 1;
        }
        if (*path == '\0') {
            
            return MP_VFS_ROOT;
        }
        for (mp_vfs_mount_t *vfs = MP_STATE_VM(vfs_mount_table); vfs != NULL; vfs = vfs->next) {
            size_t len = vfs->len - 1;
            if (len == 0) {
                *path_out = path - is_abs;
                return vfs;
            }
            if (strncmp(path, vfs->str + 1, len) == 0) {
                if (path[len] == '/') {
                    *path_out = path + len;
                    return vfs;
                } else if (path[len] == '\0') {
                    *path_out = "/";
                    return vfs;
                }
            }
        }

        
        return MP_VFS_NONE;
    }

    
    *path_out = path;
    return MP_STATE_VM(vfs_cur);
}


static mp_vfs_mount_t *lookup_path(mp_obj_t path_in, mp_obj_t *path_out) {
    const char *path = mp_obj_str_get_str(path_in);
    const char *p_out;
    mp_vfs_mount_t *vfs = mp_vfs_lookup_path(path, &p_out);
    if (vfs != MP_VFS_NONE && vfs != MP_VFS_ROOT) {
        *path_out = mp_obj_new_str_of_type(mp_obj_get_type(path_in),
            (const byte *)p_out, strlen(p_out));
    } else {
        *path_out = MP_OBJ_NULL;
    }
    return vfs;
}

static mp_obj_t mp_vfs_proxy_call(mp_vfs_mount_t *vfs, qstr meth_name, size_t n_args, const mp_obj_t *args) {
    assert(n_args <= PROXY_MAX_ARGS);
    if (vfs == MP_VFS_NONE) {
        
        mp_raise_OSError(MP_ENODEV);
    }
    if (vfs == MP_VFS_ROOT) {
        
        mp_raise_OSError(MP_EPERM);
    }
    mp_obj_t meth[2 + PROXY_MAX_ARGS];
    mp_load_method(vfs->obj, meth_name, meth);
    if (args != NULL) {
        memcpy(meth + 2, args, n_args * sizeof(*args));
    }
    return mp_call_method_n_kw(n_args, 0, meth);
}

mp_import_stat_t mp_vfs_import_stat(const char *path) {
    const char *path_out;
    mp_vfs_mount_t *vfs = mp_vfs_lookup_path(path, &path_out);
    if (vfs == MP_VFS_NONE || vfs == MP_VFS_ROOT) {
        return MP_IMPORT_STAT_NO_EXIST;
    }

    
    const mp_obj_type_t *type = mp_obj_get_type(vfs->obj);
    if (MP_OBJ_TYPE_HAS_SLOT(type, protocol)) {
        const mp_vfs_proto_t *proto = MP_OBJ_TYPE_GET_SLOT(type, protocol);
        return proto->import_stat(MP_OBJ_TO_PTR(vfs->obj), path_out);
    }

    
    mp_obj_t path_o = mp_obj_new_str(path_out, strlen(path_out));
    mp_obj_t stat;
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        stat = mp_vfs_proxy_call(vfs, MP_QSTR_stat, 1, &path_o);
        nlr_pop();
    } else {
        
        return MP_IMPORT_STAT_NO_EXIST;
    }
    mp_obj_t *items;
    mp_obj_get_array_fixed_n(stat, 10, &items);
    mp_int_t st_mode = mp_obj_get_int(items[0]);
    if (st_mode & MP_S_IFDIR) {
        return MP_IMPORT_STAT_DIR;
    } else {
        return MP_IMPORT_STAT_FILE;
    }
}

static mp_obj_t mp_vfs_autodetect(mp_obj_t bdev_obj) {
    #if MICROPY_VFS_LFS1 || MICROPY_VFS_LFS2
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        
        
        mp_vfs_blockdev_t blockdev;
        mp_vfs_blockdev_init(&blockdev, bdev_obj);
        uint8_t buf[44];
        for (size_t block_num = 0; block_num <= 1; ++block_num) {
            mp_vfs_blockdev_read_ext(&blockdev, block_num, 8, sizeof(buf), buf);
            #if MICROPY_VFS_LFS1
            if (memcmp(&buf[32], "littlefs", 8) == 0) {
                
                mp_obj_t vfs = MP_OBJ_TYPE_GET_SLOT(&mp_type_vfs_lfs1, make_new)(&mp_type_vfs_lfs1, 1, 0, &bdev_obj);
                nlr_pop();
                return vfs;
            }
            #endif
            #if MICROPY_VFS_LFS2
            if (memcmp(&buf[0], "littlefs", 8) == 0) {
                
                mp_obj_t vfs = MP_OBJ_TYPE_GET_SLOT(&mp_type_vfs_lfs2, make_new)(&mp_type_vfs_lfs2, 1, 0, &bdev_obj);
                nlr_pop();
                return vfs;
            }
            #endif
        }
        nlr_pop();
    } else {
        
    }
    #endif

    #if MICROPY_VFS_FAT
    return MP_OBJ_TYPE_GET_SLOT(&mp_fat_vfs_type, make_new)(&mp_fat_vfs_type, 1, 0, &bdev_obj);
    #endif

    
    mp_raise_OSError(MP_ENODEV);
}

mp_obj_t mp_vfs_mount(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_readonly, ARG_mkfs };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_readonly, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_rom_obj = MP_ROM_FALSE} },
        { MP_QSTR_mkfs, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_rom_obj = MP_ROM_FALSE} },
    };

    
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 2, pos_args + 2, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    
    size_t mnt_len;
    const char *mnt_str = mp_obj_str_get_data(pos_args[1], &mnt_len);

    
    mp_obj_t vfs_obj = pos_args[0];
    mp_obj_t dest[2];
    mp_load_method_maybe(vfs_obj, MP_QSTR_mount, dest);
    if (dest[0] == MP_OBJ_NULL) {
        
        
        vfs_obj = mp_vfs_autodetect(vfs_obj);
    }

    
    mp_vfs_mount_t *vfs = m_new_obj(mp_vfs_mount_t);
    vfs->str = mnt_str;
    vfs->len = mnt_len;
    vfs->obj = vfs_obj;
    vfs->next = NULL;

    
    mp_vfs_proxy_call(vfs, MP_QSTR_mount, 2, (mp_obj_t *)&args);

    
    const char *path_out;
    mp_vfs_mount_t *existing_mount = mp_vfs_lookup_path(mp_obj_str_get_str(pos_args[1]), &path_out);
    if (existing_mount != MP_VFS_NONE && existing_mount != MP_VFS_ROOT) {
        if (vfs->len != 1 && existing_mount->len == 1) {
            
        } else {
            
            mp_raise_OSError(MP_EPERM);
        }
    }

    
    mp_vfs_mount_t **vfsp = &MP_STATE_VM(vfs_mount_table);
    while (*vfsp != NULL) {
        if ((*vfsp)->len == 1) {
            
            vfs->next = *vfsp;
            break;
        }
        vfsp = &(*vfsp)->next;
    }
    *vfsp = vfs;

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_vfs_mount_obj, 2, mp_vfs_mount);

mp_obj_t mp_vfs_umount(mp_obj_t mnt_in) {
    
    mp_vfs_mount_t *vfs = NULL;
    size_t mnt_len;
    const char *mnt_str = NULL;
    if (mp_obj_is_str(mnt_in)) {
        mnt_str = mp_obj_str_get_data(mnt_in, &mnt_len);
    }
    for (mp_vfs_mount_t **vfsp = &MP_STATE_VM(vfs_mount_table); *vfsp != NULL; vfsp = &(*vfsp)->next) {
        if ((mnt_str != NULL && !memcmp(mnt_str, (*vfsp)->str, mnt_len + 1)) || (*vfsp)->obj == mnt_in) {
            vfs = *vfsp;
            *vfsp = (*vfsp)->next;
            break;
        }
    }

    if (vfs == NULL) {
        mp_raise_OSError(MP_EINVAL);
    }

    
    if (MP_STATE_VM(vfs_cur) == vfs) {
        MP_STATE_VM(vfs_cur) = MP_VFS_ROOT;
    }

    
    mp_vfs_proxy_call(vfs, MP_QSTR_umount, 0, NULL);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_vfs_umount_obj, mp_vfs_umount);


mp_obj_t mp_vfs_open(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_file, ARG_mode, ARG_encoding };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_file, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_mode, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_QSTR(MP_QSTR_r)} },
        { MP_QSTR_buffering, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_encoding, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
    };

    
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    #if MICROPY_VFS_POSIX
    
    if (mp_obj_is_small_int(args[ARG_file].u_obj)) {
        return mp_vfs_posix_file_open(&mp_type_vfs_posix_textio, args[ARG_file].u_obj, args[ARG_mode].u_obj);
    }
    #endif

    mp_vfs_mount_t *vfs = lookup_path(args[ARG_file].u_obj, &args[ARG_file].u_obj);
    return mp_vfs_proxy_call(vfs, MP_QSTR_open, 2, (mp_obj_t *)&args);
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_vfs_open_obj, 0, mp_vfs_open);

mp_obj_t mp_vfs_chdir(mp_obj_t path_in) {
    mp_obj_t path_out;
    mp_vfs_mount_t *vfs = lookup_path(path_in, &path_out);
    if (vfs == MP_VFS_ROOT) {
        
        
        
        for (vfs = MP_STATE_VM(vfs_mount_table); vfs != NULL; vfs = vfs->next) {
            if (vfs->len == 1) {
                mp_obj_t root = MP_OBJ_NEW_QSTR(MP_QSTR__slash_);
                mp_vfs_proxy_call(vfs, MP_QSTR_chdir, 1, &root);
                break;
            }
        }
        vfs = MP_VFS_ROOT;
    } else {
        mp_vfs_proxy_call(vfs, MP_QSTR_chdir, 1, &path_out);
    }
    MP_STATE_VM(vfs_cur) = vfs;
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_vfs_chdir_obj, mp_vfs_chdir);

mp_obj_t mp_vfs_getcwd(void) {
    if (MP_STATE_VM(vfs_cur) == MP_VFS_ROOT) {
        return MP_OBJ_NEW_QSTR(MP_QSTR__slash_);
    }
    mp_obj_t cwd_o = mp_vfs_proxy_call(MP_STATE_VM(vfs_cur), MP_QSTR_getcwd, 0, NULL);
    if (MP_STATE_VM(vfs_cur)->len == 1) {
        
        return cwd_o;
    }
    const char *cwd = mp_obj_str_get_str(cwd_o);
    vstr_t vstr;
    vstr_init(&vstr, MP_STATE_VM(vfs_cur)->len + strlen(cwd) + 1);
    vstr_add_strn(&vstr, MP_STATE_VM(vfs_cur)->str, MP_STATE_VM(vfs_cur)->len);
    if (!(cwd[0] == '/' && cwd[1] == 0)) {
        vstr_add_str(&vstr, cwd);
    }
    return mp_obj_new_str_from_vstr(&vstr);
}
MP_DEFINE_CONST_FUN_OBJ_0(mp_vfs_getcwd_obj, mp_vfs_getcwd);

typedef struct _mp_vfs_ilistdir_it_t {
    mp_obj_base_t base;
    mp_fun_1_t iternext;
    union {
        mp_vfs_mount_t *vfs;
        mp_obj_t iter;
    } cur;
    bool is_str;
    bool is_iter;
} mp_vfs_ilistdir_it_t;

static mp_obj_t mp_vfs_ilistdir_it_iternext(mp_obj_t self_in) {
    mp_vfs_ilistdir_it_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->is_iter) {
        
        return mp_iternext(self->cur.iter);
    } else if (self->cur.vfs == NULL) {
        
        return MP_OBJ_STOP_ITERATION;
    } else {
        
        mp_vfs_mount_t *vfs = self->cur.vfs;
        self->cur.vfs = vfs->next;
        if (vfs->len == 1) {
            
            mp_obj_t root = MP_OBJ_NEW_QSTR(MP_QSTR__slash_);
            self->is_iter = true;
            self->cur.iter = mp_vfs_proxy_call(vfs, MP_QSTR_ilistdir, 1, &root);
            return mp_iternext(self->cur.iter);
        } else {
            
            mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(3, NULL));
            t->items[0] = mp_obj_new_str_of_type(
                self->is_str ? &mp_type_str : &mp_type_bytes,
                (const byte *)vfs->str + 1, vfs->len - 1);
            t->items[1] = MP_OBJ_NEW_SMALL_INT(MP_S_IFDIR);
            t->items[2] = MP_OBJ_NEW_SMALL_INT(0); 
            return MP_OBJ_FROM_PTR(t);
        }
    }
}

mp_obj_t mp_vfs_ilistdir(size_t n_args, const mp_obj_t *args) {
    mp_obj_t path_in;
    if (n_args == 1) {
        path_in = args[0];
    } else {
        path_in = MP_OBJ_NEW_QSTR(MP_QSTR_);
    }

    mp_obj_t path_out;
    mp_vfs_mount_t *vfs = lookup_path(path_in, &path_out);

    if (vfs == MP_VFS_ROOT) {
        
        mp_vfs_ilistdir_it_t *iter = mp_obj_malloc(mp_vfs_ilistdir_it_t, &mp_type_polymorph_iter);
        iter->iternext = mp_vfs_ilistdir_it_iternext;
        iter->cur.vfs = MP_STATE_VM(vfs_mount_table);
        iter->is_str = mp_obj_get_type(path_in) == &mp_type_str;
        iter->is_iter = false;
        return MP_OBJ_FROM_PTR(iter);
    }

    return mp_vfs_proxy_call(vfs, MP_QSTR_ilistdir, 1, &path_out);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_vfs_ilistdir_obj, 0, 1, mp_vfs_ilistdir);

mp_obj_t mp_vfs_listdir(size_t n_args, const mp_obj_t *args) {
    mp_obj_t iter = mp_vfs_ilistdir(n_args, args);
    mp_obj_t dir_list = mp_obj_new_list(0, NULL);
    mp_obj_t next;
    while ((next = mp_iternext(iter)) != MP_OBJ_STOP_ITERATION) {
        mp_obj_list_append(dir_list, mp_obj_subscr(next, MP_OBJ_NEW_SMALL_INT(0), MP_OBJ_SENTINEL));
    }
    return dir_list;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_vfs_listdir_obj, 0, 1, mp_vfs_listdir);

mp_obj_t mp_vfs_mkdir(mp_obj_t path_in) {
    mp_obj_t path_out;
    mp_vfs_mount_t *vfs = lookup_path(path_in, &path_out);
    if (vfs == MP_VFS_ROOT || (vfs != MP_VFS_NONE && !strcmp(mp_obj_str_get_str(path_out), "/"))) {
        mp_raise_OSError(MP_EEXIST);
    }
    return mp_vfs_proxy_call(vfs, MP_QSTR_mkdir, 1, &path_out);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_vfs_mkdir_obj, mp_vfs_mkdir);

mp_obj_t mp_vfs_remove(mp_obj_t path_in) {
    mp_obj_t path_out;
    mp_vfs_mount_t *vfs = lookup_path(path_in, &path_out);
    return mp_vfs_proxy_call(vfs, MP_QSTR_remove, 1, &path_out);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_vfs_remove_obj, mp_vfs_remove);

mp_obj_t mp_vfs_rename(mp_obj_t old_path_in, mp_obj_t new_path_in) {
    mp_obj_t args[2];
    mp_vfs_mount_t *old_vfs = lookup_path(old_path_in, &args[0]);
    mp_vfs_mount_t *new_vfs = lookup_path(new_path_in, &args[1]);
    if (old_vfs != new_vfs) {
        
        mp_raise_OSError(MP_EPERM);
    }
    return mp_vfs_proxy_call(old_vfs, MP_QSTR_rename, 2, args);
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_vfs_rename_obj, mp_vfs_rename);

mp_obj_t mp_vfs_rmdir(mp_obj_t path_in) {
    mp_obj_t path_out;
    mp_vfs_mount_t *vfs = lookup_path(path_in, &path_out);
    return mp_vfs_proxy_call(vfs, MP_QSTR_rmdir, 1, &path_out);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_vfs_rmdir_obj, mp_vfs_rmdir);

mp_obj_t mp_vfs_stat(mp_obj_t path_in) {
    mp_obj_t path_out;
    mp_vfs_mount_t *vfs = lookup_path(path_in, &path_out);
    if (vfs == MP_VFS_ROOT) {
        mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(10, NULL));
        t->items[0] = MP_OBJ_NEW_SMALL_INT(MP_S_IFDIR); 
        for (int i = 1; i <= 9; ++i) {
            t->items[i] = MP_OBJ_NEW_SMALL_INT(0); 
        }
        return MP_OBJ_FROM_PTR(t);
    }
    return mp_vfs_proxy_call(vfs, MP_QSTR_stat, 1, &path_out);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_vfs_stat_obj, mp_vfs_stat);

mp_obj_t mp_vfs_statvfs(mp_obj_t path_in) {
    mp_obj_t path_out;
    mp_vfs_mount_t *vfs = lookup_path(path_in, &path_out);
    if (vfs == MP_VFS_ROOT) {
        
        for (vfs = MP_STATE_VM(vfs_mount_table); vfs != NULL; vfs = vfs->next) {
            if (vfs->len == 1) {
                break;
            }
        }

        
        if (vfs == NULL) {
            mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(10, NULL));

            
            for (int i = 0; i <= 8; ++i) {
                t->items[i] = MP_OBJ_NEW_SMALL_INT(0);
            }

            
            t->items[9] = MP_OBJ_NEW_SMALL_INT(MICROPY_ALLOC_PATH_MAX);

            return MP_OBJ_FROM_PTR(t);
        }

        
        path_out = MP_OBJ_NEW_QSTR(MP_QSTR__slash_);
    }
    return mp_vfs_proxy_call(vfs, MP_QSTR_statvfs, 1, &path_out);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_vfs_statvfs_obj, mp_vfs_statvfs);


int mp_vfs_mount_and_chdir_protected(mp_obj_t bdev, mp_obj_t mount_point) {
    nlr_buf_t nlr;
    mp_int_t ret = -MP_EIO;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t args[] = { bdev, mount_point };
        mp_vfs_mount(2, args, (mp_map_t *)&mp_const_empty_map);
        mp_vfs_chdir(mount_point);
        ret = 0; 
        nlr_pop();
    } else {
        mp_obj_base_t *exc = nlr.ret_val;
        if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(exc->type), MP_OBJ_FROM_PTR(&mp_type_OSError))) {
            mp_obj_t v = mp_obj_exception_get_value(MP_OBJ_FROM_PTR(exc));
            mp_obj_get_int_maybe(v, &ret); 
            ret = -ret;
        }
    }
    return ret;
}

MP_REGISTER_ROOT_POINTER(struct _mp_vfs_mount_t *vfs_cur);
MP_REGISTER_ROOT_POINTER(struct _mp_vfs_mount_t *vfs_mount_table);

#endif 
