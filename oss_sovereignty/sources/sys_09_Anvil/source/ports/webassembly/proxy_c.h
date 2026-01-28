
#ifndef MICROPY_INCLUDED_WEBASSEMBLY_PROXY_C_H
#define MICROPY_INCLUDED_WEBASSEMBLY_PROXY_C_H

#include "py/obj.h"


#define PVN (3)

typedef struct _mp_obj_jsproxy_t {
    mp_obj_base_t base;
    int ref;
} mp_obj_jsproxy_t;

extern const mp_obj_type_t mp_type_jsproxy;
extern const mp_obj_type_t mp_type_JsException;

void external_call_depth_inc(void);
void external_call_depth_dec(void);

void proxy_c_init(void);
mp_obj_t proxy_convert_js_to_mp_obj_cside(uint32_t *value);
void proxy_convert_mp_to_js_obj_cside(mp_obj_t obj, uint32_t *out);
void proxy_convert_mp_to_js_exc_cside(void *exc, uint32_t *out);

mp_obj_t mp_obj_new_jsproxy(int ref);
void mp_obj_jsproxy_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest);

static inline bool mp_obj_is_jsproxy(mp_obj_t o) {
    return mp_obj_get_type(o) == &mp_type_jsproxy;
}

static inline int mp_obj_jsproxy_get_ref(mp_obj_t o) {
    mp_obj_jsproxy_t *self = MP_OBJ_TO_PTR(o);
    return self->ref;
}

#endif 
