
#ifndef MICROPY_INCLUDED_PY_QSTR_H
#define MICROPY_INCLUDED_PY_QSTR_H

#include "py/mpconfig.h"
#include "py/misc.h"








enum {
    #ifndef NO_QSTR
#define QDEF0(id, hash, len, str) id,
#define QDEF1(id, hash, len, str)
    #include "genhdr/qstrdefs.generated.h"
#undef QDEF0
#undef QDEF1
    #endif
    MP_QSTRnumber_of_static,
    MP_QSTRstart_of_main = MP_QSTRnumber_of_static - 1, 

    #ifndef NO_QSTR
#define QDEF0(id, hash, len, str)
#define QDEF1(id, hash, len, str) id,
    #include "genhdr/qstrdefs.generated.h"
#undef QDEF0
#undef QDEF1
    #endif
    MP_QSTRnumber_of, 
};

typedef size_t qstr;
typedef uint16_t qstr_short_t;

#if MICROPY_QSTR_BYTES_IN_HASH == 0

#elif MICROPY_QSTR_BYTES_IN_HASH == 1
typedef uint8_t qstr_hash_t;
#elif MICROPY_QSTR_BYTES_IN_HASH == 2
typedef uint16_t qstr_hash_t;
#else
#error unimplemented qstr hash decoding
#endif

#if MICROPY_QSTR_BYTES_IN_LEN == 1
typedef uint8_t qstr_len_t;
#elif MICROPY_QSTR_BYTES_IN_LEN == 2
typedef uint16_t qstr_len_t;
#else
#error unimplemented qstr length decoding
#endif

typedef struct _qstr_pool_t {
    const struct _qstr_pool_t *prev;
    size_t total_prev_len : (8 * sizeof(size_t) - 1);
    size_t is_sorted : 1;
    size_t alloc;
    size_t len;
    #if MICROPY_QSTR_BYTES_IN_HASH
    qstr_hash_t *hashes;
    #endif
    qstr_len_t *lengths;
    const char *qstrs[];
} qstr_pool_t;

#define QSTR_TOTAL() (MP_STATE_VM(last_pool)->total_prev_len + MP_STATE_VM(last_pool)->len)

void qstr_init(void);

size_t qstr_compute_hash(const byte *data, size_t len);

qstr qstr_find_strn(const char *str, size_t str_len); 

qstr qstr_from_str(const char *str);
qstr qstr_from_strn(const char *str, size_t len);

mp_uint_t qstr_hash(qstr q);
const char *qstr_str(qstr q);
size_t qstr_len(qstr q);
const byte *qstr_data(qstr q, size_t *len);

void qstr_pool_info(size_t *n_pool, size_t *n_qstr, size_t *n_str_data_bytes, size_t *n_total_bytes);
void qstr_dump_data(void);

#if MICROPY_ROM_TEXT_COMPRESSION
void mp_decompress_rom_string(byte *dst, const mp_rom_error_text_t src);
#define MP_IS_COMPRESSED_ROM_STRING(s) (*(byte *)(s) == 0xff)
#endif

#endif 
