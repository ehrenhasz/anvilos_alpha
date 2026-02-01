 

#include <string.h>

#include "py/runtime.h"



#define SWAP(type, var1, var2) { type t = var2; var2 = var1; var1 = t; }



void mp_seq_multiply(const void *items, size_t item_sz, size_t len, size_t times, void *dest) {
    for (size_t i = 0; i < times; i++) {
        size_t copy_sz = item_sz * len;
        memcpy(dest, items, copy_sz);
        dest = (char *)dest + copy_sz;
    }
}

#if MICROPY_PY_BUILTINS_SLICE

bool mp_seq_get_fast_slice_indexes(mp_uint_t len, mp_obj_t slice, mp_bound_slice_t *indexes) {
    mp_obj_slice_indices(slice, len, indexes);

    
    if (indexes->step < 0) {
        indexes->stop++;
    }

    
    if (indexes->step > 0 && indexes->start > indexes->stop) {
        indexes->stop = indexes->start;
    } else if (indexes->step < 0 && indexes->start < indexes->stop) {
        indexes->stop = indexes->start + 1;
    }

    return indexes->step == 1;
}

#endif

mp_obj_t mp_seq_extract_slice(size_t len, const mp_obj_t *seq, mp_bound_slice_t *indexes) {
    (void)len; 

    mp_int_t start = indexes->start, stop = indexes->stop;
    mp_int_t step = indexes->step;

    mp_obj_t res = mp_obj_new_list(0, NULL);

    if (step < 0) {
        while (start >= stop) {
            mp_obj_list_append(res, seq[start]);
            start += step;
        }
    } else {
        while (start < stop) {
            mp_obj_list_append(res, seq[start]);
            start += step;
        }
    }
    return res;
}



bool mp_seq_cmp_bytes(mp_uint_t op, const byte *data1, size_t len1, const byte *data2, size_t len2) {
    if (op == MP_BINARY_OP_EQUAL && len1 != len2) {
        return false;
    }

    
    if (op == MP_BINARY_OP_LESS || op == MP_BINARY_OP_LESS_EQUAL) {
        SWAP(const byte *, data1, data2);
        SWAP(size_t, len1, len2);
        if (op == MP_BINARY_OP_LESS) {
            op = MP_BINARY_OP_MORE;
        } else {
            op = MP_BINARY_OP_MORE_EQUAL;
        }
    }
    size_t min_len = len1 < len2 ? len1 : len2;
    int res = memcmp(data1, data2, min_len);
    if (op == MP_BINARY_OP_EQUAL) {
        
        return res == 0;
    }
    if (res < 0) {
        return false;
    }
    if (res > 0) {
        return true;
    }

    
    
    if (len1 != len2) {
        if (len1 < len2) {
            
            return false;
        }
    } else if (op == MP_BINARY_OP_MORE) {
        
        return false;
    }
    return true;
}



bool mp_seq_cmp_objs(mp_uint_t op, const mp_obj_t *items1, size_t len1, const mp_obj_t *items2, size_t len2) {
    if (op == MP_BINARY_OP_EQUAL && len1 != len2) {
        return false;
    }

    
    if (op == MP_BINARY_OP_LESS || op == MP_BINARY_OP_LESS_EQUAL) {
        SWAP(const mp_obj_t *, items1, items2);
        SWAP(size_t, len1, len2);
        if (op == MP_BINARY_OP_LESS) {
            op = MP_BINARY_OP_MORE;
        } else {
            op = MP_BINARY_OP_MORE_EQUAL;
        }
    }

    size_t len = len1 < len2 ? len1 : len2;
    for (size_t i = 0; i < len; i++) {
        
        if (mp_obj_equal(items1[i], items2[i])) {
            continue;
        }

        
        if (op == MP_BINARY_OP_EQUAL) {
            
            return false;
        }

        
        return mp_binary_op(op, items1[i], items2[i]) == mp_const_true;
    }

    
    
    if (len1 != len2) {
        if (len1 < len2) {
            
            return false;
        }
    } else if (op == MP_BINARY_OP_MORE) {
        
        return false;
    }

    return true;
}


mp_obj_t mp_seq_index_obj(const mp_obj_t *items, size_t len, size_t n_args, const mp_obj_t *args) {
    const mp_obj_type_t *type = mp_obj_get_type(args[0]);
    mp_obj_t value = args[1];
    size_t start = 0;
    size_t stop = len;

    if (n_args >= 3) {
        start = mp_get_index(type, len, args[2], true);
        if (n_args >= 4) {
            stop = mp_get_index(type, len, args[3], true);
        }
    }

    for (size_t i = start; i < stop; i++) {
        if (mp_obj_equal(items[i], value)) {
            
            return MP_OBJ_NEW_SMALL_INT(i);
        }
    }

    mp_raise_ValueError(MP_ERROR_TEXT("object not in sequence"));
}

mp_obj_t mp_seq_count_obj(const mp_obj_t *items, size_t len, mp_obj_t value) {
    size_t count = 0;
    for (size_t i = 0; i < len; i++) {
        if (mp_obj_equal(items[i], value)) {
            count++;
        }
    }

    
    return MP_OBJ_NEW_SMALL_INT(count);
}
