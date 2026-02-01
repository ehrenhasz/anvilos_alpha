 

#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>

#include "py/objlist.h"
#include "py/objstr.h"
#include "py/objtuple.h"
#include "py/objtype.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/mperrno.h"

#if MICROPY_ROM_TEXT_COMPRESSION && !defined(NO_QSTR)


#define MP_MATCH_COMPRESSED(...) 
#define MP_COMPRESSED_DATA(...) 
#include "genhdr/compressed.data.h"
#undef MP_MATCH_COMPRESSED
#undef MP_COMPRESSED_DATA
#endif


#define TRACEBACK_ENTRY_LEN (3)



#if MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF








#define EMG_BUF_TRACEBACK_OFFSET    (0)
#define EMG_BUF_TRACEBACK_SIZE      (2 * TRACEBACK_ENTRY_LEN * sizeof(size_t))
#define EMG_BUF_TUPLE_OFFSET        (EMG_BUF_TRACEBACK_OFFSET + EMG_BUF_TRACEBACK_SIZE)
#define EMG_BUF_TUPLE_SIZE(n_args)  (sizeof(mp_obj_tuple_t) + n_args * sizeof(mp_obj_t))
#define EMG_BUF_STR_OFFSET          (EMG_BUF_TUPLE_OFFSET + EMG_BUF_TUPLE_SIZE(1))
#define EMG_BUF_STR_BUF_OFFSET      (EMG_BUF_STR_OFFSET + sizeof(mp_obj_str_t))

#if MICROPY_EMERGENCY_EXCEPTION_BUF_SIZE > 0
#define mp_emergency_exception_buf_size MICROPY_EMERGENCY_EXCEPTION_BUF_SIZE

void mp_init_emergency_exception_buf(void) {
    
    
    
}

#else
#define mp_emergency_exception_buf_size MP_STATE_VM(mp_emergency_exception_buf_size)

#include "py/mphal.h" 

void mp_init_emergency_exception_buf(void) {
    mp_emergency_exception_buf_size = 0;
    MP_STATE_VM(mp_emergency_exception_buf) = NULL;
}

mp_obj_t mp_alloc_emergency_exception_buf(mp_obj_t size_in) {
    mp_int_t size = mp_obj_get_int(size_in);
    void *buf = NULL;
    if (size > 0) {
        buf = m_new(byte, size);
    }

    int old_size = mp_emergency_exception_buf_size;
    void *old_buf = MP_STATE_VM(mp_emergency_exception_buf);

    
    
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_emergency_exception_buf_size = size;
    MP_STATE_VM(mp_emergency_exception_buf) = buf;
    MICROPY_END_ATOMIC_SECTION(atomic_state);

    if (old_buf != NULL) {
        m_del(byte, old_buf, old_size);
    }
    return mp_const_none;
}
#endif
#endif  

bool mp_obj_is_native_exception_instance(mp_obj_t self_in) {
    return MP_OBJ_TYPE_GET_SLOT_OR_NULL(mp_obj_get_type(self_in), make_new) == mp_obj_exception_make_new;
}

static mp_obj_exception_t *get_native_exception(mp_obj_t self_in) {
    assert(mp_obj_is_exception_instance(self_in));
    if (mp_obj_is_native_exception_instance(self_in)) {
        return MP_OBJ_TO_PTR(self_in);
    } else {
        return MP_OBJ_TO_PTR(((mp_obj_instance_t *)MP_OBJ_TO_PTR(self_in))->subobj[0]);
    }
}

static void decompress_error_text_maybe(mp_obj_exception_t *o) {
    #if MICROPY_ROM_TEXT_COMPRESSION
    if (o->args->len == 1 && mp_obj_is_exact_type(o->args->items[0], &mp_type_str)) {
        mp_obj_str_t *o_str = MP_OBJ_TO_PTR(o->args->items[0]);
        if (MP_IS_COMPRESSED_ROM_STRING(o_str->data)) {
            byte *buf = m_new_maybe(byte, MP_MAX_UNCOMPRESSED_TEXT_LEN + 1);
            if (!buf) {
                #if MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF
                
                buf = (byte *)((uint8_t *)MP_STATE_VM(mp_emergency_exception_buf) + EMG_BUF_STR_BUF_OFFSET);
                size_t avail = (uint8_t *)MP_STATE_VM(mp_emergency_exception_buf) + mp_emergency_exception_buf_size - buf;
                if (avail < MP_MAX_UNCOMPRESSED_TEXT_LEN + 1) {
                    
                    o->args = (mp_obj_tuple_t *)&mp_const_empty_tuple_obj;
                    return;
                }
                #else
                o->args = (mp_obj_tuple_t *)&mp_const_empty_tuple_obj;
                return;
                #endif
            }
            mp_decompress_rom_string(buf, (mp_rom_error_text_t)o_str->data);
            o_str->data = buf;
            o_str->len = strlen((const char *)buf);
            o_str->hash = 0;
        }
        
        if (o_str->hash == 0) {
            o_str->hash = qstr_compute_hash(o_str->data, o_str->len);
        }
    }
    #endif
}

void mp_obj_exception_print(const mp_print_t *print, mp_obj_t o_in, mp_print_kind_t kind) {
    mp_obj_exception_t *o = MP_OBJ_TO_PTR(o_in);
    mp_print_kind_t k = kind & ~PRINT_EXC_SUBCLASS;
    bool is_subclass = kind & PRINT_EXC_SUBCLASS;
    if (!is_subclass && (k == PRINT_REPR || k == PRINT_EXC)) {
        mp_print_str(print, qstr_str(o->base.type->name));
    }

    if (k == PRINT_EXC) {
        mp_print_str(print, ": ");
    }

    decompress_error_text_maybe(o);

    if (k == PRINT_STR || k == PRINT_EXC) {
        if (o->args == NULL || o->args->len == 0) {
            mp_print_str(print, "");
            return;
        }

        #if MICROPY_PY_ERRNO
        
        if (o->base.type == &mp_type_OSError && o->args->len > 0 && o->args->len < 3 && mp_obj_is_small_int(o->args->items[0])) {
            qstr qst = mp_errno_to_str(o->args->items[0]);
            if (qst != MP_QSTRnull) {
                mp_printf(print, "[Errno " INT_FMT "] %q", MP_OBJ_SMALL_INT_VALUE(o->args->items[0]), qst);
                if (o->args->len > 1) {
                    mp_print_str(print, ": ");
                    mp_obj_print_helper(print, o->args->items[1], PRINT_STR);
                }
                return;
            }
        }
        #endif

        if (o->args->len == 1) {
            mp_obj_print_helper(print, o->args->items[0], PRINT_STR);
            return;
        }
    }

    mp_obj_tuple_print(print, MP_OBJ_FROM_PTR(o->args), kind);
}

mp_obj_t mp_obj_exception_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, MP_OBJ_FUN_ARGS_MAX, false);

    
    mp_obj_exception_t *o_exc = m_new_obj_maybe(mp_obj_exception_t);
    if (o_exc == NULL) {
        o_exc = &MP_STATE_VM(mp_emergency_exception_obj);
    }

    
    o_exc->base.type = type;
    o_exc->traceback_data = NULL;

    mp_obj_tuple_t *o_tuple;
    if (n_args == 0) {
        
        o_tuple = (mp_obj_tuple_t *)&mp_const_empty_tuple_obj;
    } else {
        
        o_tuple = m_new_obj_var_maybe(mp_obj_tuple_t, items, mp_obj_t, n_args);

        #if MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF
        
        
        
        if (o_tuple == NULL && mp_emergency_exception_buf_size >=
            (mp_int_t)(EMG_BUF_TUPLE_OFFSET + EMG_BUF_TUPLE_SIZE(n_args))) {
            o_tuple = (mp_obj_tuple_t *)
                ((uint8_t *)MP_STATE_VM(mp_emergency_exception_buf) + EMG_BUF_TUPLE_OFFSET);
        }
        #endif

        if (o_tuple == NULL) {
            
            o_tuple = (mp_obj_tuple_t *)&mp_const_empty_tuple_obj;
        } else {
            
            o_tuple->base.type = &mp_type_tuple;
            o_tuple->len = n_args;
            memcpy(o_tuple->items, args, n_args * sizeof(mp_obj_t));
        }
    }

    
    o_exc->args = o_tuple;

    return MP_OBJ_FROM_PTR(o_exc);
}


mp_obj_t mp_obj_exception_get_value(mp_obj_t self_in) {
    mp_obj_exception_t *self = get_native_exception(self_in);
    if (self->args->len == 0) {
        return mp_const_none;
    } else {
        decompress_error_text_maybe(self);
        return self->args->items[0];
    }
}

void mp_obj_exception_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    mp_obj_exception_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] != MP_OBJ_NULL) {
        
        if (attr == MP_QSTR___traceback__ && dest[1] == mp_const_none) {
            
            
            
            
            
            
            self->traceback_len = 0;
            dest[0] = MP_OBJ_NULL; 
        }
        return;
    }
    if (attr == MP_QSTR_args) {
        decompress_error_text_maybe(self);
        dest[0] = MP_OBJ_FROM_PTR(self->args);
    } else if (attr == MP_QSTR_value || attr == MP_QSTR_errno) {
        
        
        dest[0] = mp_obj_exception_get_value(self_in);
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_BaseException,
    MP_QSTR_BaseException,
    MP_TYPE_FLAG_NONE,
    make_new, mp_obj_exception_make_new,
    print, mp_obj_exception_print,
    attr, mp_obj_exception_attr
    );





MP_DEFINE_EXCEPTION(SystemExit, BaseException)
MP_DEFINE_EXCEPTION(KeyboardInterrupt, BaseException)
MP_DEFINE_EXCEPTION(GeneratorExit, BaseException)
MP_DEFINE_EXCEPTION(Exception, BaseException)
  #if MICROPY_PY_ASYNC_AWAIT
  MP_DEFINE_EXCEPTION(StopAsyncIteration, Exception)
  #endif
  MP_DEFINE_EXCEPTION(StopIteration, Exception)
  MP_DEFINE_EXCEPTION(ArithmeticError, Exception)
    
    MP_DEFINE_EXCEPTION(OverflowError, ArithmeticError)
    MP_DEFINE_EXCEPTION(ZeroDivisionError, ArithmeticError)
  MP_DEFINE_EXCEPTION(AssertionError, Exception)
  MP_DEFINE_EXCEPTION(AttributeError, Exception)
  
  MP_DEFINE_EXCEPTION(EOFError, Exception)
  MP_DEFINE_EXCEPTION(ImportError, Exception)
  MP_DEFINE_EXCEPTION(LookupError, Exception)
    MP_DEFINE_EXCEPTION(IndexError, LookupError)
    MP_DEFINE_EXCEPTION(KeyError, LookupError)
  MP_DEFINE_EXCEPTION(MemoryError, Exception)
  MP_DEFINE_EXCEPTION(NameError, Exception)
     
  MP_DEFINE_EXCEPTION(OSError, Exception)
     
  MP_DEFINE_EXCEPTION(RuntimeError, Exception)
    MP_DEFINE_EXCEPTION(NotImplementedError, RuntimeError)
  MP_DEFINE_EXCEPTION(SyntaxError, Exception)
    MP_DEFINE_EXCEPTION(IndentationError, SyntaxError)
     
  
  MP_DEFINE_EXCEPTION(TypeError, Exception)
#if MICROPY_EMIT_NATIVE
    MP_DEFINE_EXCEPTION(ViperTypeError, TypeError)
#endif
  MP_DEFINE_EXCEPTION(ValueError, Exception)
#if MICROPY_PY_BUILTINS_STR_UNICODE
    MP_DEFINE_EXCEPTION(UnicodeError, ValueError)
    
#endif
   



mp_obj_t mp_obj_new_exception(const mp_obj_type_t *exc_type) {
    assert(MP_OBJ_TYPE_GET_SLOT_OR_NULL(exc_type, make_new) == mp_obj_exception_make_new);
    return mp_obj_exception_make_new(exc_type, 0, 0, NULL);
}

mp_obj_t mp_obj_new_exception_args(const mp_obj_type_t *exc_type, size_t n_args, const mp_obj_t *args) {
    assert(MP_OBJ_TYPE_GET_SLOT_OR_NULL(exc_type, make_new) == mp_obj_exception_make_new);
    return mp_obj_exception_make_new(exc_type, n_args, 0, args);
}

#if MICROPY_ERROR_REPORTING != MICROPY_ERROR_REPORTING_NONE

mp_obj_t mp_obj_new_exception_msg(const mp_obj_type_t *exc_type, mp_rom_error_text_t msg) {
    
    assert(MP_OBJ_TYPE_GET_SLOT_OR_NULL(exc_type, make_new) == mp_obj_exception_make_new);

    
    mp_obj_str_t *o_str = m_new_obj_maybe(mp_obj_str_t);

    #if MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF
    
    
    
    if (o_str == NULL
        && mp_emergency_exception_buf_size >= (mp_int_t)(EMG_BUF_STR_OFFSET + sizeof(mp_obj_str_t))) {
        o_str = (mp_obj_str_t *)((uint8_t *)MP_STATE_VM(mp_emergency_exception_buf)
            + EMG_BUF_STR_OFFSET);
    }
    #endif

    if (o_str == NULL) {
        
        return mp_obj_exception_make_new(exc_type, 0, 0, NULL);
    }

    
    o_str->base.type = &mp_type_str;
    o_str->len = strlen((const char *)msg);
    o_str->data = (const byte *)msg;
    #if MICROPY_ROM_TEXT_COMPRESSION
    o_str->hash = 0; 
    #else
    o_str->hash = qstr_compute_hash(o_str->data, o_str->len);
    #endif
    mp_obj_t arg = MP_OBJ_FROM_PTR(o_str);
    return mp_obj_exception_make_new(exc_type, 1, 0, &arg);
}





struct _exc_printer_t {
    bool allow_realloc;
    size_t alloc;
    size_t len;
    byte *buf;
};

static void exc_add_strn(void *data, const char *str, size_t len) {
    struct _exc_printer_t *pr = data;
    if (pr->len + len >= pr->alloc) {
        
        if (pr->allow_realloc) {
            size_t new_alloc = pr->alloc + len + 16;
            byte *new_buf = m_renew_maybe(byte, pr->buf, pr->alloc, new_alloc, true);
            if (new_buf == NULL) {
                pr->allow_realloc = false;
                len = pr->alloc - pr->len - 1;
            } else {
                pr->alloc = new_alloc;
                pr->buf = new_buf;
            }
        } else {
            len = pr->alloc - pr->len - 1;
        }
    }
    memcpy(pr->buf + pr->len, str, len);
    pr->len += len;
}

mp_obj_t mp_obj_new_exception_msg_varg(const mp_obj_type_t *exc_type, mp_rom_error_text_t fmt, ...) {
    va_list args;
    va_start(args, fmt);
    mp_obj_t exc = mp_obj_new_exception_msg_vlist(exc_type, fmt, args);
    va_end(args);
    return exc;
}

mp_obj_t mp_obj_new_exception_msg_vlist(const mp_obj_type_t *exc_type, mp_rom_error_text_t fmt, va_list args) {
    assert(fmt != NULL);

    
    assert(MP_OBJ_TYPE_GET_SLOT_OR_NULL(exc_type, make_new) == mp_obj_exception_make_new);

    
    mp_obj_str_t *o_str = m_new_obj_maybe(mp_obj_str_t);
    size_t o_str_alloc = strlen((const char *)fmt) + 1;
    byte *o_str_buf = m_new_maybe(byte, o_str_alloc);

    bool used_emg_buf = false;
    #if MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF
    
    
    
    if ((o_str == NULL || o_str_buf == NULL)
        && mp_emergency_exception_buf_size >= (mp_int_t)(EMG_BUF_STR_OFFSET + sizeof(mp_obj_str_t) + 16)) {
        used_emg_buf = true;
        o_str = (mp_obj_str_t *)((uint8_t *)MP_STATE_VM(mp_emergency_exception_buf) + EMG_BUF_STR_OFFSET);
        o_str_buf = (byte *)((uint8_t *)MP_STATE_VM(mp_emergency_exception_buf) + EMG_BUF_STR_BUF_OFFSET);
        o_str_alloc = (uint8_t *)MP_STATE_VM(mp_emergency_exception_buf) + mp_emergency_exception_buf_size - o_str_buf;
    }
    #endif

    if (o_str == NULL) {
        
        
        return mp_obj_exception_make_new(exc_type, 0, 0, NULL);
    }

    if (o_str_buf == NULL) {
        
        
        
        
        o_str->len = o_str_alloc - 1; 
        o_str->data = (const byte *)fmt;
    } else {
        
        
        struct _exc_printer_t exc_pr = {!used_emg_buf, o_str_alloc, 0, o_str_buf};
        mp_print_t print = {&exc_pr, exc_add_strn};
        const char *fmt2 = (const char *)fmt;
        #if MICROPY_ROM_TEXT_COMPRESSION
        byte decompressed[MP_MAX_UNCOMPRESSED_TEXT_LEN];
        if (MP_IS_COMPRESSED_ROM_STRING(fmt)) {
            mp_decompress_rom_string(decompressed, fmt);
            fmt2 = (const char *)decompressed;
        }
        #endif
        mp_vprintf(&print, fmt2, args);
        exc_pr.buf[exc_pr.len] = '\0';
        o_str->len = exc_pr.len;
        o_str->data = exc_pr.buf;
    }

    
    o_str->base.type = &mp_type_str;
    #if MICROPY_ROM_TEXT_COMPRESSION
    o_str->hash = 0; 
    #else
    o_str->hash = qstr_compute_hash(o_str->data, o_str->len);
    #endif
    mp_obj_t arg = MP_OBJ_FROM_PTR(o_str);
    return mp_obj_exception_make_new(exc_type, 1, 0, &arg);
}

#endif


bool mp_obj_is_exception_type(mp_obj_t self_in) {
    if (mp_obj_is_type(self_in, &mp_type_type)) {
        
        mp_obj_type_t *self = MP_OBJ_TO_PTR(self_in);
        if (MP_OBJ_TYPE_GET_SLOT_OR_NULL(self, make_new) == mp_obj_exception_make_new) {
            return true;
        }
    }
    return mp_obj_is_subclass_fast(self_in, MP_OBJ_FROM_PTR(&mp_type_BaseException));
}


bool mp_obj_is_exception_instance(mp_obj_t self_in) {
    return mp_obj_is_exception_type(MP_OBJ_FROM_PTR(mp_obj_get_type(self_in)));
}




bool mp_obj_exception_match(mp_obj_t exc, mp_const_obj_t exc_type) {
    
    if (mp_obj_is_exception_instance(exc)) {
        exc = MP_OBJ_FROM_PTR(mp_obj_get_type(exc));
    }
    return mp_obj_is_subclass_fast(exc, exc_type);
}



void mp_obj_exception_clear_traceback(mp_obj_t self_in) {
    mp_obj_exception_t *self = get_native_exception(self_in);
    
    
    self->traceback_data = NULL;
}

void mp_obj_exception_add_traceback(mp_obj_t self_in, qstr file, size_t line, qstr block) {
    mp_obj_exception_t *self = get_native_exception(self_in);

    
    

    #if MICROPY_PY_SYS_TRACEBACKLIMIT
    mp_int_t max_traceback = MP_OBJ_SMALL_INT_VALUE(MP_STATE_VM(sys_mutable[MP_SYS_MUTABLE_TRACEBACKLIMIT]));
    if (max_traceback <= 0) {
        return;
    } else if (self->traceback_data != NULL && self->traceback_len >= max_traceback * TRACEBACK_ENTRY_LEN) {
        self->traceback_len -= TRACEBACK_ENTRY_LEN;
        memmove(self->traceback_data, self->traceback_data + TRACEBACK_ENTRY_LEN, self->traceback_len * sizeof(self->traceback_data[0]));
    }
    #endif

    if (self->traceback_data == NULL) {
        self->traceback_data = m_new_maybe(size_t, TRACEBACK_ENTRY_LEN);
        if (self->traceback_data == NULL) {
            #if MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF
            if (mp_emergency_exception_buf_size >= (mp_int_t)(EMG_BUF_TRACEBACK_OFFSET + EMG_BUF_TRACEBACK_SIZE)) {
                
                size_t *tb = (size_t *)((uint8_t *)MP_STATE_VM(mp_emergency_exception_buf)
                    + EMG_BUF_TRACEBACK_OFFSET);
                self->traceback_data = tb;
                self->traceback_alloc = EMG_BUF_TRACEBACK_SIZE / sizeof(size_t);
            } else {
                
                return;
            }
            #else
            
            return;
            #endif
        } else {
            
            self->traceback_alloc = TRACEBACK_ENTRY_LEN;
        }
        self->traceback_len = 0;
    } else if (self->traceback_len + TRACEBACK_ENTRY_LEN > self->traceback_alloc) {
        #if MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF
        if (self->traceback_data == (size_t *)MP_STATE_VM(mp_emergency_exception_buf)) {
            
            return;
        }
        #endif
        
        size_t *tb_data = m_renew_maybe(size_t, self->traceback_data, self->traceback_alloc,
            self->traceback_alloc + TRACEBACK_ENTRY_LEN, true);
        if (tb_data == NULL) {
            return;
        }
        self->traceback_data = tb_data;
        self->traceback_alloc += TRACEBACK_ENTRY_LEN;
    }

    size_t *tb_data = &self->traceback_data[self->traceback_len];
    self->traceback_len += TRACEBACK_ENTRY_LEN;
    tb_data[0] = file;
    tb_data[1] = line;
    tb_data[2] = block;
}

void mp_obj_exception_get_traceback(mp_obj_t self_in, size_t *n, size_t **values) {
    mp_obj_exception_t *self = get_native_exception(self_in);

    if (self->traceback_data == NULL) {
        *n = 0;
        *values = NULL;
    } else {
        *n = self->traceback_len;
        *values = self->traceback_data;
    }
}
