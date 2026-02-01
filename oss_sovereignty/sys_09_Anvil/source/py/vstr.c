 

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "py/mpconfig.h"
#include "py/runtime.h"
#include "py/mpprint.h"


#define ROUND_ALLOC(a) (((a) & ((~0U) - 7)) + 8)


void vstr_init(vstr_t *vstr, size_t alloc) {
    if (alloc < 1) {
        alloc = 1;
    }
    vstr->alloc = alloc;
    vstr->len = 0;
    vstr->buf = m_new(char, vstr->alloc);
    vstr->fixed_buf = false;
}



void vstr_init_len(vstr_t *vstr, size_t len) {
    vstr_init(vstr, len + 1);
    vstr->len = len;
}

void vstr_init_fixed_buf(vstr_t *vstr, size_t alloc, char *buf) {
    vstr->alloc = alloc;
    vstr->len = 0;
    vstr->buf = buf;
    vstr->fixed_buf = true;
}

void vstr_init_print(vstr_t *vstr, size_t alloc, mp_print_t *print) {
    vstr_init(vstr, alloc);
    print->data = vstr;
    print->print_strn = (mp_print_strn_t)vstr_add_strn;
}

void vstr_clear(vstr_t *vstr) {
    if (!vstr->fixed_buf) {
        m_del(char, vstr->buf, vstr->alloc);
    }
    vstr->buf = NULL;
}

vstr_t *vstr_new(size_t alloc) {
    vstr_t *vstr = m_new_obj(vstr_t);
    vstr_init(vstr, alloc);
    return vstr;
}

void vstr_free(vstr_t *vstr) {
    if (vstr != NULL) {
        if (!vstr->fixed_buf) {
            m_del(char, vstr->buf, vstr->alloc);
        }
        m_del_obj(vstr_t, vstr);
    }
}


char *vstr_extend(vstr_t *vstr, size_t size) {
    if (vstr->fixed_buf) {
        
        
        mp_raise_msg(&mp_type_RuntimeError, NULL);
    }
    char *new_buf = m_renew(char, vstr->buf, vstr->alloc, vstr->alloc + size);
    char *p = new_buf + vstr->alloc;
    vstr->alloc += size;
    vstr->buf = new_buf;
    return p;
}

static void vstr_ensure_extra(vstr_t *vstr, size_t size) {
    if (vstr->len + size > vstr->alloc) {
        if (vstr->fixed_buf) {
            
            
            mp_raise_msg(&mp_type_RuntimeError, NULL);
        }
        size_t new_alloc = ROUND_ALLOC((vstr->len + size) + 16);
        char *new_buf = m_renew(char, vstr->buf, vstr->alloc, new_alloc);
        vstr->alloc = new_alloc;
        vstr->buf = new_buf;
    }
}

void vstr_hint_size(vstr_t *vstr, size_t size) {
    vstr_ensure_extra(vstr, size);
}

char *vstr_add_len(vstr_t *vstr, size_t len) {
    vstr_ensure_extra(vstr, len);
    char *buf = vstr->buf + vstr->len;
    vstr->len += len;
    return buf;
}


char *vstr_null_terminated_str(vstr_t *vstr) {
    
    if (vstr->alloc == vstr->len) {
        vstr_extend(vstr, 1);
    }
    vstr->buf[vstr->len] = '\0';
    return vstr->buf;
}

void vstr_add_byte(vstr_t *vstr, byte b) {
    byte *buf = (byte *)vstr_add_len(vstr, 1);
    buf[0] = b;
}

void vstr_add_char(vstr_t *vstr, unichar c) {
    #if MICROPY_PY_BUILTINS_STR_UNICODE
    
    
    if (c < 0x80) {
        byte *buf = (byte *)vstr_add_len(vstr, 1);
        *buf = (byte)c;
    } else if (c < 0x800) {
        byte *buf = (byte *)vstr_add_len(vstr, 2);
        buf[0] = (c >> 6) | 0xC0;
        buf[1] = (c & 0x3F) | 0x80;
    } else if (c < 0x10000) {
        byte *buf = (byte *)vstr_add_len(vstr, 3);
        buf[0] = (c >> 12) | 0xE0;
        buf[1] = ((c >> 6) & 0x3F) | 0x80;
        buf[2] = (c & 0x3F) | 0x80;
    } else {
        assert(c < 0x110000);
        byte *buf = (byte *)vstr_add_len(vstr, 4);
        buf[0] = (c >> 18) | 0xF0;
        buf[1] = ((c >> 12) & 0x3F) | 0x80;
        buf[2] = ((c >> 6) & 0x3F) | 0x80;
        buf[3] = (c & 0x3F) | 0x80;
    }
    #else
    vstr_add_byte(vstr, c);
    #endif
}

void vstr_add_str(vstr_t *vstr, const char *str) {
    vstr_add_strn(vstr, str, strlen(str));
}

void vstr_add_strn(vstr_t *vstr, const char *str, size_t len) {
    vstr_ensure_extra(vstr, len);
    memmove(vstr->buf + vstr->len, str, len);
    vstr->len += len;
}

static char *vstr_ins_blank_bytes(vstr_t *vstr, size_t byte_pos, size_t byte_len) {
    size_t l = vstr->len;
    if (byte_pos > l) {
        byte_pos = l;
    }
    if (byte_len > 0) {
        
        vstr_ensure_extra(vstr, byte_len);
        
        memmove(vstr->buf + byte_pos + byte_len, vstr->buf + byte_pos, l - byte_pos);
        
        vstr->len += byte_len;
    }
    return vstr->buf + byte_pos;
}

void vstr_ins_byte(vstr_t *vstr, size_t byte_pos, byte b) {
    char *s = vstr_ins_blank_bytes(vstr, byte_pos, 1);
    *s = b;
}

void vstr_ins_char(vstr_t *vstr, size_t char_pos, unichar chr) {
    
    char *s = vstr_ins_blank_bytes(vstr, char_pos, 1);
    *s = chr;
}

void vstr_cut_head_bytes(vstr_t *vstr, size_t bytes_to_cut) {
    vstr_cut_out_bytes(vstr, 0, bytes_to_cut);
}

void vstr_cut_tail_bytes(vstr_t *vstr, size_t len) {
    if (len > vstr->len) {
        vstr->len = 0;
    } else {
        vstr->len -= len;
    }
}

void vstr_cut_out_bytes(vstr_t *vstr, size_t byte_pos, size_t bytes_to_cut) {
    if (byte_pos >= vstr->len) {
        return;
    } else if (byte_pos + bytes_to_cut >= vstr->len) {
        vstr->len = byte_pos;
    } else {
        memmove(vstr->buf + byte_pos, vstr->buf + byte_pos + bytes_to_cut, vstr->len - byte_pos - bytes_to_cut);
        vstr->len -= bytes_to_cut;
    }
}

void vstr_printf(vstr_t *vstr, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vstr_vprintf(vstr, fmt, ap);
    va_end(ap);
}

void vstr_vprintf(vstr_t *vstr, const char *fmt, va_list ap) {
    mp_print_t print = {vstr, (mp_print_strn_t)vstr_add_strn};
    mp_vprintf(&print, fmt, ap);
}
