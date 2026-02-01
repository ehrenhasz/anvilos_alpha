 

#if MICROPY_PY_OPENAMP_REMOTEPROC

#include "py/obj.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "extmod/vfs.h"

#include "metal/sys.h"
#include "metal/alloc.h"
#include "metal/errno.h"
#include "metal/io.h"

#include "openamp/open_amp.h"
#include "openamp/remoteproc.h"
#include "openamp/remoteproc_loader.h"

#include "modopenamp.h"
#include "modopenamp_remoteproc.h"

#if !MICROPY_PY_OPENAMP
#error "MICROPY_PY_OPENAMP_REMOTEPROC requires MICROPY_PY_OPENAMP"
#endif

typedef struct openamp_remoteproc_obj {
    mp_obj_base_t base;
    struct remoteproc rproc;
} openamp_remoteproc_obj_t;

const mp_obj_type_t openamp_remoteproc_type;


extern struct image_store_ops openamp_remoteproc_store_ops;


const struct remoteproc_ops openamp_remoteproc_ops = {
    .init = mp_openamp_remoteproc_init,
    .mmap = mp_openamp_remoteproc_mmap,
    .start = mp_openamp_remoteproc_start,
    .stop = mp_openamp_remoteproc_stop,
    .config = mp_openamp_remoteproc_config,
    .remove = mp_openamp_remoteproc_remove,
    .shutdown = mp_openamp_remoteproc_shutdown,
};

static mp_obj_t openamp_remoteproc_start(mp_obj_t self_in) {
    openamp_remoteproc_obj_t *self = MP_OBJ_TO_PTR(self_in);

    
    int error = remoteproc_start(&self->rproc);
    if (error != 0) {
        self->rproc.state = RPROC_ERROR;
        mp_raise_OSError(error);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(openamp_remoteproc_start_obj, openamp_remoteproc_start);

static mp_obj_t openamp_remoteproc_stop(mp_obj_t self_in) {
    openamp_remoteproc_obj_t *self = MP_OBJ_TO_PTR(self_in);

    
    int error = remoteproc_stop(&self->rproc);
    if (error != 0) {
        mp_raise_OSError(error);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(openamp_remoteproc_stop_obj, openamp_remoteproc_stop);

static mp_obj_t openamp_remoteproc_shutdown(mp_obj_t self_in) {
    openamp_remoteproc_obj_t *self = MP_OBJ_TO_PTR(self_in);

    
    int error = remoteproc_shutdown(&self->rproc);
    if (error != 0) {
        mp_raise_OSError(error);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(openamp_remoteproc_shutdown_obj, openamp_remoteproc_shutdown);

mp_obj_t openamp_remoteproc_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_entry };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_entry, MP_ARG_OBJ | MP_ARG_REQUIRED,  {.u_rom_obj = MP_ROM_NONE } },
    };

    
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    openamp_remoteproc_obj_t *self = mp_obj_malloc_with_finaliser(openamp_remoteproc_obj_t, &openamp_remoteproc_type);

    
    if (MP_STATE_PORT(virtio_device) == NULL) {
        openamp_init();
    }

    
    
    
    remoteproc_init(&self->rproc, &openamp_remoteproc_ops, NULL);

    
    int error = remoteproc_config(&self->rproc, NULL);
    if (error != 0) {
        mp_raise_OSError(error);
    }

    if (mp_obj_is_int(args[ARG_entry].u_obj)) {
        self->rproc.bootaddr = mp_obj_get_int(args[ARG_entry].u_obj);
    } else {
        #if MICROPY_PY_OPENAMP_REMOTEPROC_ELFLD_ENABLE
        
        const char *path = mp_obj_str_get_str(args[ARG_entry].u_obj);
        int error = remoteproc_load(&self->rproc, path, self->rproc.priv, &openamp_remoteproc_store_ops, NULL);
        if (error != 0) {
            mp_raise_OSError(error);
        }
        #else
        mp_raise_TypeError(MP_ERROR_TEXT("loading firmware is not supported."));
        #endif
    }
    return MP_OBJ_FROM_PTR(self);
}

static const mp_rom_map_elem_t openamp_remoteproc_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_RemoteProc) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&openamp_remoteproc_shutdown_obj) },
    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&openamp_remoteproc_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&openamp_remoteproc_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_shutdown), MP_ROM_PTR(&openamp_remoteproc_shutdown_obj) },
};
static MP_DEFINE_CONST_DICT(openamp_remoteproc_dict, openamp_remoteproc_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    openamp_remoteproc_type,
    MP_QSTR_RemoteProc,
    MP_TYPE_FLAG_NONE,
    make_new, openamp_remoteproc_make_new,
    locals_dict, &openamp_remoteproc_dict
    );

#endif 
