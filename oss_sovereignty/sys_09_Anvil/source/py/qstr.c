 

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "py/mpstate.h"
#include "py/qstr.h"
#include "py/gc.h"
#include "py/runtime.h"

#if MICROPY_DEBUG_VERBOSE 
#define DEBUG_printf DEBUG_printf
#else 
#define DEBUG_printf(...) (void)0
#endif




#if MICROPY_QSTR_BYTES_IN_HASH
#define Q_HASH_MASK ((1 << (8 * MICROPY_QSTR_BYTES_IN_HASH)) - 1)
#else
#define Q_HASH_MASK (0xffff)
#endif

#if MICROPY_PY_THREAD && !MICROPY_PY_THREAD_GIL
#define QSTR_ENTER() mp_thread_mutex_lock(&MP_STATE_VM(qstr_mutex), 1)
#define QSTR_EXIT() mp_thread_mutex_unlock(&MP_STATE_VM(qstr_mutex))
#else
#define QSTR_ENTER()
#define QSTR_EXIT()
#endif



#define MICROPY_ALLOC_QSTR_ENTRIES_INIT (10)


size_t qstr_compute_hash(const byte *data, size_t len) {
    
    size_t hash = 5381;
    for (const byte *top = data + len; data < top; data++) {
        hash = ((hash << 5) + hash) ^ (*data); 
    }
    hash &= Q_HASH_MASK;
    
    if (hash == 0) {
        hash++;
    }
    return hash;
}







#if MICROPY_QSTR_BYTES_IN_HASH
const qstr_hash_t mp_qstr_const_hashes_static[] = {
    #ifndef NO_QSTR
#define QDEF0(id, hash, len, str) hash,
#define QDEF1(id, hash, len, str)
    #include "genhdr/qstrdefs.generated.h"
#undef QDEF0
#undef QDEF1
    #endif
};
#endif

const qstr_len_t mp_qstr_const_lengths_static[] = {
    #ifndef NO_QSTR
#define QDEF0(id, hash, len, str) len,
#define QDEF1(id, hash, len, str)
    #include "genhdr/qstrdefs.generated.h"
#undef QDEF0
#undef QDEF1
    #endif
};

const qstr_pool_t mp_qstr_const_pool_static = {
    NULL,               
    0,                  
    false,              
    MICROPY_ALLOC_QSTR_ENTRIES_INIT,
    MP_QSTRnumber_of_static,   
    #if MICROPY_QSTR_BYTES_IN_HASH
    (qstr_hash_t *)mp_qstr_const_hashes_static,
    #endif
    (qstr_len_t *)mp_qstr_const_lengths_static,
    {
        #ifndef NO_QSTR
#define QDEF0(id, hash, len, str) str,
#define QDEF1(id, hash, len, str)
        #include "genhdr/qstrdefs.generated.h"
#undef QDEF0
#undef QDEF1
        #endif
    },
};



#if MICROPY_QSTR_BYTES_IN_HASH
const qstr_hash_t mp_qstr_const_hashes[] = {
    #ifndef NO_QSTR
#define QDEF0(id, hash, len, str)
#define QDEF1(id, hash, len, str) hash,
    #include "genhdr/qstrdefs.generated.h"
#undef QDEF0
#undef QDEF1
    #endif
};
#endif

const qstr_len_t mp_qstr_const_lengths[] = {
    #ifndef NO_QSTR
#define QDEF0(id, hash, len, str)
#define QDEF1(id, hash, len, str) len,
    #include "genhdr/qstrdefs.generated.h"
#undef QDEF0
#undef QDEF1
    #endif
};

const qstr_pool_t mp_qstr_const_pool = {
    &mp_qstr_const_pool_static,
    MP_QSTRnumber_of_static,
    true,               
    MICROPY_ALLOC_QSTR_ENTRIES_INIT,
    MP_QSTRnumber_of - MP_QSTRnumber_of_static,   
    #if MICROPY_QSTR_BYTES_IN_HASH
    (qstr_hash_t *)mp_qstr_const_hashes,
    #endif
    (qstr_len_t *)mp_qstr_const_lengths,
    {
        #ifndef NO_QSTR
#define QDEF0(id, hash, len, str)
#define QDEF1(id, hash, len, str) str,
        #include "genhdr/qstrdefs.generated.h"
#undef QDEF0
#undef QDEF1
        #endif
    },
};



#ifdef MICROPY_QSTR_EXTRA_POOL
extern const qstr_pool_t MICROPY_QSTR_EXTRA_POOL;
#define CONST_POOL MICROPY_QSTR_EXTRA_POOL
#else
#define CONST_POOL mp_qstr_const_pool
#endif

void qstr_init(void) {
    MP_STATE_VM(last_pool) = (qstr_pool_t *)&CONST_POOL; 
    MP_STATE_VM(qstr_last_chunk) = NULL;

    #if MICROPY_PY_THREAD && !MICROPY_PY_THREAD_GIL
    mp_thread_mutex_init(&MP_STATE_VM(qstr_mutex));
    #endif
}

static const qstr_pool_t *find_qstr(qstr *q) {
    
    
    const qstr_pool_t *pool = MP_STATE_VM(last_pool);
    while (*q < pool->total_prev_len) {
        pool = pool->prev;
    }
    *q -= pool->total_prev_len;
    assert(*q < pool->len);
    return pool;
}


static qstr qstr_add(mp_uint_t len, const char *q_ptr) {
    #if MICROPY_QSTR_BYTES_IN_HASH
    mp_uint_t hash = qstr_compute_hash((const byte *)q_ptr, len);
    DEBUG_printf("QSTR: add hash=%d len=%d data=%.*s\n", hash, len, len, q_ptr);
    #else
    DEBUG_printf("QSTR: add len=%d data=%.*s\n", len, len, q_ptr);
    #endif

    
    if (MP_STATE_VM(last_pool)->len >= MP_STATE_VM(last_pool)->alloc) {
        size_t new_alloc = MP_STATE_VM(last_pool)->alloc * 2;
        #ifdef MICROPY_QSTR_EXTRA_POOL
        
        new_alloc = MAX(MICROPY_ALLOC_QSTR_ENTRIES_INIT, new_alloc);
        #endif
        mp_uint_t pool_size = sizeof(qstr_pool_t)
            + (sizeof(const char *)
                #if MICROPY_QSTR_BYTES_IN_HASH
                + sizeof(qstr_hash_t)
                #endif
                + sizeof(qstr_len_t)) * new_alloc;
        qstr_pool_t *pool = (qstr_pool_t *)m_malloc_maybe(pool_size);
        if (pool == NULL) {
            
            
            
            
            
            MP_STATE_VM(qstr_last_chunk) = NULL;
            QSTR_EXIT();
            m_malloc_fail(new_alloc);
        }
        #if MICROPY_QSTR_BYTES_IN_HASH
        pool->hashes = (qstr_hash_t *)(pool->qstrs + new_alloc);
        pool->lengths = (qstr_len_t *)(pool->hashes + new_alloc);
        #else
        pool->lengths = (qstr_len_t *)(pool->qstrs + new_alloc);
        #endif
        pool->prev = MP_STATE_VM(last_pool);
        pool->total_prev_len = MP_STATE_VM(last_pool)->total_prev_len + MP_STATE_VM(last_pool)->len;
        pool->alloc = new_alloc;
        pool->len = 0;
        MP_STATE_VM(last_pool) = pool;
        DEBUG_printf("QSTR: allocate new pool of size %d\n", MP_STATE_VM(last_pool)->alloc);
    }

    
    mp_uint_t at = MP_STATE_VM(last_pool)->len;
    #if MICROPY_QSTR_BYTES_IN_HASH
    MP_STATE_VM(last_pool)->hashes[at] = hash;
    #endif
    MP_STATE_VM(last_pool)->lengths[at] = len;
    MP_STATE_VM(last_pool)->qstrs[at] = q_ptr;
    MP_STATE_VM(last_pool)->len++;

    
    return MP_STATE_VM(last_pool)->total_prev_len + at;
}

qstr qstr_find_strn(const char *str, size_t str_len) {
    if (str_len == 0) {
        
        return MP_QSTR_;
    }

    #if MICROPY_QSTR_BYTES_IN_HASH
    
    size_t str_hash = qstr_compute_hash((const byte *)str, str_len);
    #endif

    
    for (const qstr_pool_t *pool = MP_STATE_VM(last_pool); pool != NULL; pool = pool->prev) {
        size_t low = 0;
        size_t high = pool->len - 1;

        
        if (pool->is_sorted) {
            while (high - low > 1) {
                size_t mid = (low + high) / 2;
                int cmp = strncmp(str, pool->qstrs[mid], str_len);
                if (cmp <= 0) {
                    high = mid;
                } else {
                    low = mid;
                }
            }
        }

        
        for (mp_uint_t at = low; at < high + 1; at++) {
            if (
                #if MICROPY_QSTR_BYTES_IN_HASH
                pool->hashes[at] == str_hash &&
                #endif
                pool->lengths[at] == str_len
                && memcmp(pool->qstrs[at], str, str_len) == 0) {
                return pool->total_prev_len + at;
            }
        }
    }

    
    return MP_QSTRnull;
}

qstr qstr_from_str(const char *str) {
    return qstr_from_strn(str, strlen(str));
}

qstr qstr_from_strn(const char *str, size_t len) {
    QSTR_ENTER();
    qstr q = qstr_find_strn(str, len);
    if (q == 0) {
        

        
        if (len >= (1 << (8 * MICROPY_QSTR_BYTES_IN_LEN))) {
            QSTR_EXIT();
            mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("name too long"));
        }

        
        size_t n_bytes = len + 1;

        if (MP_STATE_VM(qstr_last_chunk) != NULL && MP_STATE_VM(qstr_last_used) + n_bytes > MP_STATE_VM(qstr_last_alloc)) {
            
            char *new_p = m_renew_maybe(char, MP_STATE_VM(qstr_last_chunk), MP_STATE_VM(qstr_last_alloc), MP_STATE_VM(qstr_last_alloc) + n_bytes, false);
            if (new_p == NULL) {
                
                (void)m_renew_maybe(char, MP_STATE_VM(qstr_last_chunk), MP_STATE_VM(qstr_last_alloc), MP_STATE_VM(qstr_last_used), false);
                MP_STATE_VM(qstr_last_chunk) = NULL;
            } else {
                
                MP_STATE_VM(qstr_last_alloc) += n_bytes;
            }
        }

        if (MP_STATE_VM(qstr_last_chunk) == NULL) {
            
            size_t al = n_bytes;
            if (al < MICROPY_ALLOC_QSTR_CHUNK_INIT) {
                al = MICROPY_ALLOC_QSTR_CHUNK_INIT;
            }
            MP_STATE_VM(qstr_last_chunk) = m_new_maybe(char, al);
            if (MP_STATE_VM(qstr_last_chunk) == NULL) {
                
                MP_STATE_VM(qstr_last_chunk) = m_new_maybe(char, n_bytes);
                if (MP_STATE_VM(qstr_last_chunk) == NULL) {
                    QSTR_EXIT();
                    m_malloc_fail(n_bytes);
                }
                al = n_bytes;
            }
            MP_STATE_VM(qstr_last_alloc) = al;
            MP_STATE_VM(qstr_last_used) = 0;
        }

        
        char *q_ptr = MP_STATE_VM(qstr_last_chunk) + MP_STATE_VM(qstr_last_used);
        MP_STATE_VM(qstr_last_used) += n_bytes;

        
        memcpy(q_ptr, str, len);
        q_ptr[len] = '\0';
        q = qstr_add(len, q_ptr);
    }
    QSTR_EXIT();
    return q;
}

mp_uint_t qstr_hash(qstr q) {
    const qstr_pool_t *pool = find_qstr(&q);
    #if MICROPY_QSTR_BYTES_IN_HASH
    return pool->hashes[q];
    #else
    return qstr_compute_hash((byte *)pool->qstrs[q], pool->lengths[q]);
    #endif
}

size_t qstr_len(qstr q) {
    const qstr_pool_t *pool = find_qstr(&q);
    return pool->lengths[q];
}

const char *qstr_str(qstr q) {
    const qstr_pool_t *pool = find_qstr(&q);
    return pool->qstrs[q];
}

const byte *qstr_data(qstr q, size_t *len) {
    const qstr_pool_t *pool = find_qstr(&q);
    *len = pool->lengths[q];
    return (byte *)pool->qstrs[q];
}

void qstr_pool_info(size_t *n_pool, size_t *n_qstr, size_t *n_str_data_bytes, size_t *n_total_bytes) {
    QSTR_ENTER();
    *n_pool = 0;
    *n_qstr = 0;
    *n_str_data_bytes = 0;
    *n_total_bytes = 0;
    for (const qstr_pool_t *pool = MP_STATE_VM(last_pool); pool != NULL && pool != &CONST_POOL; pool = pool->prev) {
        *n_pool += 1;
        *n_qstr += pool->len;
        for (qstr_len_t *l = pool->lengths, *l_top = pool->lengths + pool->len; l < l_top; l++) {
            *n_str_data_bytes += *l + 1;
        }
        #if MICROPY_ENABLE_GC
        *n_total_bytes += gc_nbytes(pool); 
        #else
        *n_total_bytes += sizeof(qstr_pool_t)
            + (sizeof(const char *)
                #if MICROPY_QSTR_BYTES_IN_HASH
                + sizeof(qstr_hash_t)
                #endif
                + sizeof(qstr_len_t)) * pool->alloc;
        #endif
    }
    *n_total_bytes += *n_str_data_bytes;
    QSTR_EXIT();
}

#if MICROPY_PY_MICROPYTHON_MEM_INFO
void qstr_dump_data(void) {
    QSTR_ENTER();
    for (const qstr_pool_t *pool = MP_STATE_VM(last_pool); pool != NULL && pool != &CONST_POOL; pool = pool->prev) {
        for (const char *const *q = pool->qstrs, *const *q_top = pool->qstrs + pool->len; q < q_top; q++) {
            mp_printf(&mp_plat_print, "Q(%s)\n", *q);
        }
    }
    QSTR_EXIT();
}
#endif

#if MICROPY_ROM_TEXT_COMPRESSION

#ifdef NO_QSTR




#else


#define MP_COMPRESSED_DATA(x) static const char *compressed_string_data = x;
#define MP_MATCH_COMPRESSED(a, b)
#include "genhdr/compressed.data.h"
#undef MP_COMPRESSED_DATA
#undef MP_MATCH_COMPRESSED

#endif 







static const byte *find_uncompressed_string(uint8_t n) {
    const byte *c = (byte *)compressed_string_data;
    while (n > 0) {
        while ((*c & 0x80) == 0) {
            ++c;
        }
        ++c;
        --n;
    }
    return c;
}



void mp_decompress_rom_string(byte *dst, const mp_rom_error_text_t src_chr) {
    
    const byte *src = (byte *)src_chr + 1;
    
    
    int state = 0;
    while (*src) {
        if ((byte) * src >= 128) {
            if (state != 0) {
                *dst++ = ' ';
            }
            state = 1;

            
            const byte *word = find_uncompressed_string(*src & 0x7f);
            
            while ((*word & 0x80) == 0) {
                *dst++ = *word++;
            }
            *dst++ = (*word & 0x7f);
        } else {
            
            if (state == 1) {
                *dst++ = ' ';
            }
            state = 2;

            *dst++ = *src;
        }
        ++src;
    }
    
    *dst = 0;
}

#endif 
