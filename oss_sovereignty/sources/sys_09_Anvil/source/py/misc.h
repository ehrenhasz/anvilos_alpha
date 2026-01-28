
#ifndef MICROPY_INCLUDED_PY_MISC_H
#define MICROPY_INCLUDED_PY_MISC_H





#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef unsigned char byte;
typedef unsigned int uint;



#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif


#define MP_STRINGIFY_HELPER(x) #x
#define MP_STRINGIFY(x) MP_STRINGIFY_HELPER(x)


#define MP_STATIC_ASSERT(cond) ((void)sizeof(char[1 - 2 * !(cond)]))





#if defined(_MSC_VER) || defined(__cplusplus)
#define MP_STATIC_ASSERT_NONCONSTEXPR(cond) ((void)1)
#else
#define MP_STATIC_ASSERT_NONCONSTEXPR(cond) MP_STATIC_ASSERT(cond)
#endif


#define MP_CEIL_DIVIDE(a, b) (((a) + (b) - 1) / (b))
#define MP_ROUND_DIVIDE(a, b) (((a) + (b) / 2) / (b))





#define m_new(type, num) ((type *)(m_malloc(sizeof(type) * (num))))
#define m_new_maybe(type, num) ((type *)(m_malloc_maybe(sizeof(type) * (num))))
#define m_new0(type, num) ((type *)(m_malloc0(sizeof(type) * (num))))
#define m_new_obj(type) (m_new(type, 1))
#define m_new_obj_maybe(type) (m_new_maybe(type, 1))
#define m_new_obj_var(obj_type, var_field, var_type, var_num) ((obj_type *)m_malloc(offsetof(obj_type, var_field) + sizeof(var_type) * (var_num)))
#define m_new_obj_var0(obj_type, var_field, var_type, var_num) ((obj_type *)m_malloc0(offsetof(obj_type, var_field) + sizeof(var_type) * (var_num)))
#define m_new_obj_var_maybe(obj_type, var_field, var_type, var_num) ((obj_type *)m_malloc_maybe(offsetof(obj_type, var_field) + sizeof(var_type) * (var_num)))
#if MICROPY_MALLOC_USES_ALLOCATED_SIZE
#define m_renew(type, ptr, old_num, new_num) ((type *)(m_realloc((ptr), sizeof(type) * (old_num), sizeof(type) * (new_num))))
#define m_renew_maybe(type, ptr, old_num, new_num, allow_move) ((type *)(m_realloc_maybe((ptr), sizeof(type) * (old_num), sizeof(type) * (new_num), (allow_move))))
#define m_del(type, ptr, num) m_free(ptr, sizeof(type) * (num))
#define m_del_var(obj_type, var_field, var_type, var_num, ptr) (m_free(ptr, offsetof(obj_type, var_field) + sizeof(var_type) * (var_num)))
#else
#define m_renew(type, ptr, old_num, new_num) ((type *)(m_realloc((ptr), sizeof(type) * (new_num))))
#define m_renew_maybe(type, ptr, old_num, new_num, allow_move) ((type *)(m_realloc_maybe((ptr), sizeof(type) * (new_num), (allow_move))))
#define m_del(type, ptr, num) ((void)(num), m_free(ptr))
#define m_del_var(obj_type, var_field, var_type, var_num, ptr) ((void)(var_num), m_free(ptr))
#endif
#define m_del_obj(type, ptr) (m_del(type, ptr, 1))

void *m_malloc(size_t num_bytes);
void *m_malloc_maybe(size_t num_bytes);
void *m_malloc_with_finaliser(size_t num_bytes);
void *m_malloc0(size_t num_bytes);
#if MICROPY_MALLOC_USES_ALLOCATED_SIZE
void *m_realloc(void *ptr, size_t old_num_bytes, size_t new_num_bytes);
void *m_realloc_maybe(void *ptr, size_t old_num_bytes, size_t new_num_bytes, bool allow_move);
void m_free(void *ptr, size_t num_bytes);
#else
void *m_realloc(void *ptr, size_t new_num_bytes);
void *m_realloc_maybe(void *ptr, size_t new_num_bytes, bool allow_move);
void m_free(void *ptr);
#endif
NORETURN void m_malloc_fail(size_t num_bytes);

#if MICROPY_TRACKED_ALLOC


void *m_tracked_calloc(size_t nmemb, size_t size);
void m_tracked_free(void *ptr_in);
#endif

#if MICROPY_MEM_STATS
size_t m_get_total_bytes_allocated(void);
size_t m_get_current_bytes_allocated(void);
size_t m_get_peak_bytes_allocated(void);
#endif




#define MP_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))


#define MP_ALIGN(ptr, alignment) (void *)(((uintptr_t)(ptr) + ((alignment) - 1)) & ~((alignment) - 1))



#if MICROPY_PY_BUILTINS_STR_UNICODE

typedef uint32_t unichar;
#else


typedef uint unichar;
#endif

#if MICROPY_PY_BUILTINS_STR_UNICODE
unichar utf8_get_char(const byte *s);
const byte *utf8_next_char(const byte *s);
size_t utf8_charlen(const byte *str, size_t len);
#else
static inline unichar utf8_get_char(const byte *s) {
    return *s;
}
static inline const byte *utf8_next_char(const byte *s) {
    return s + 1;
}
static inline size_t utf8_charlen(const byte *str, size_t len) {
    (void)str;
    return len;
}
#endif

bool unichar_isspace(unichar c);
bool unichar_isalpha(unichar c);
bool unichar_isprint(unichar c);
bool unichar_isdigit(unichar c);
bool unichar_isxdigit(unichar c);
bool unichar_isident(unichar c);
bool unichar_isalnum(unichar c);
bool unichar_isupper(unichar c);
bool unichar_islower(unichar c);
unichar unichar_tolower(unichar c);
unichar unichar_toupper(unichar c);
mp_uint_t unichar_xdigit_value(unichar c);
#define UTF8_IS_NONASCII(ch) ((ch) & 0x80)
#define UTF8_IS_CONT(ch) (((ch) & 0xC0) == 0x80)



typedef struct _vstr_t {
    size_t alloc;
    size_t len;
    char *buf;
    bool fixed_buf;
} vstr_t;


#define VSTR_FIXED(vstr, alloc) vstr_t vstr; char vstr##_buf[(alloc)]; vstr_init_fixed_buf(&vstr, (alloc), vstr##_buf);

void vstr_init(vstr_t *vstr, size_t alloc);
void vstr_init_len(vstr_t *vstr, size_t len);
void vstr_init_fixed_buf(vstr_t *vstr, size_t alloc, char *buf);
struct _mp_print_t;
void vstr_init_print(vstr_t *vstr, size_t alloc, struct _mp_print_t *print);
void vstr_clear(vstr_t *vstr);
vstr_t *vstr_new(size_t alloc);
void vstr_free(vstr_t *vstr);
static inline void vstr_reset(vstr_t *vstr) {
    vstr->len = 0;
}
static inline char *vstr_str(vstr_t *vstr) {
    return vstr->buf;
}
static inline size_t vstr_len(vstr_t *vstr) {
    return vstr->len;
}
void vstr_hint_size(vstr_t *vstr, size_t size);
char *vstr_extend(vstr_t *vstr, size_t size);
char *vstr_add_len(vstr_t *vstr, size_t len);
char *vstr_null_terminated_str(vstr_t *vstr);
void vstr_add_byte(vstr_t *vstr, byte v);
void vstr_add_char(vstr_t *vstr, unichar chr);
void vstr_add_str(vstr_t *vstr, const char *str);
void vstr_add_strn(vstr_t *vstr, const char *str, size_t len);
void vstr_ins_byte(vstr_t *vstr, size_t byte_pos, byte b);
void vstr_ins_char(vstr_t *vstr, size_t char_pos, unichar chr);
void vstr_cut_head_bytes(vstr_t *vstr, size_t bytes_to_cut);
void vstr_cut_tail_bytes(vstr_t *vstr, size_t bytes_to_cut);
void vstr_cut_out_bytes(vstr_t *vstr, size_t byte_pos, size_t bytes_to_cut);
void vstr_printf(vstr_t *vstr, const char *fmt, ...);



#define CHECKBUF(buf, max_size) char buf[max_size + 1]; size_t buf##_len = max_size; char *buf##_p = buf;
#define CHECKBUF_RESET(buf, max_size) buf##_len = max_size; buf##_p = buf;
#define CHECKBUF_APPEND(buf, src, src_len) \
    { size_t l = MIN(src_len, buf##_len); \
      memcpy(buf##_p, src, l); \
      buf##_len -= l; \
      buf##_p += l; }
#define CHECKBUF_APPEND_0(buf) { *buf##_p = 0; }
#define CHECKBUF_LEN(buf) (buf##_p - buf)

#ifdef va_start
void vstr_vprintf(vstr_t *vstr, const char *fmt, va_list ap);
#endif


int DEBUG_printf(const char *fmt, ...);

extern mp_uint_t mp_verbose_flag;



#if MICROPY_PY_BUILTINS_FLOAT

#if MICROPY_FLOAT_IMPL == MICROPY_FLOAT_IMPL_DOUBLE
#define MP_FLOAT_EXP_BITS (11)
#define MP_FLOAT_EXP_OFFSET (1023)
#define MP_FLOAT_FRAC_BITS (52)
typedef uint64_t mp_float_uint_t;
#elif MICROPY_FLOAT_IMPL == MICROPY_FLOAT_IMPL_FLOAT
#define MP_FLOAT_EXP_BITS (8)
#define MP_FLOAT_EXP_OFFSET (127)
#define MP_FLOAT_FRAC_BITS (23)
typedef uint32_t mp_float_uint_t;
#endif

#define MP_FLOAT_EXP_BIAS ((1 << (MP_FLOAT_EXP_BITS - 1)) - 1)

typedef union _mp_float_union_t {
    mp_float_t f;
    #if MP_ENDIANNESS_LITTLE
    struct {
        mp_float_uint_t frc : MP_FLOAT_FRAC_BITS;
        mp_float_uint_t exp : MP_FLOAT_EXP_BITS;
        mp_float_uint_t sgn : 1;
    } p;
    #else
    struct {
        mp_float_uint_t sgn : 1;
        mp_float_uint_t exp : MP_FLOAT_EXP_BITS;
        mp_float_uint_t frc : MP_FLOAT_FRAC_BITS;
    } p;
    #endif
    mp_float_uint_t i;
} mp_float_union_t;

#endif 



#if MICROPY_ROM_TEXT_COMPRESSION

#if MICROPY_ERROR_REPORTING == MICROPY_ERROR_REPORTING_NONE
#error "MICROPY_ERROR_REPORTING_NONE requires MICROPY_ROM_TEXT_COMPRESSION disabled"
#endif

#ifdef NO_QSTR




#else





typedef struct {
    #if defined(__clang__) || defined(_MSC_VER)
    
    
    char dummy;
    #endif
} *mp_rom_error_text_t;

#include <string.h>

inline MP_ALWAYSINLINE const char *MP_COMPRESSED_ROM_TEXT(const char *msg) {
    
    
    #define MP_MATCH_COMPRESSED(a, b) if (strcmp(msg, a) == 0) { return b; } else

    
    #define MP_COMPRESSED_DATA(x)

    #include "genhdr/compressed.data.h"

#undef MP_COMPRESSED_DATA
#undef MP_MATCH_COMPRESSED

    return msg;
}

#endif

#else



typedef const char *mp_rom_error_text_t;
#define MP_COMPRESSED_ROM_TEXT(x) x

#endif 



#define MP_ERROR_TEXT(x) (mp_rom_error_text_t)MP_COMPRESSED_ROM_TEXT(x)

#endif 
