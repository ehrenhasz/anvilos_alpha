 

#include <assert.h>
#include <string.h>

#include "py/obj.h"
#include "py/misc.h"
#include "py/asmbase.h"

#if MICROPY_EMIT_MACHINE_CODE

void mp_asm_base_init(mp_asm_base_t *as, size_t max_num_labels) {
    as->max_num_labels = max_num_labels;
    as->label_offsets = m_new(size_t, max_num_labels);
}

void mp_asm_base_deinit(mp_asm_base_t *as, bool free_code) {
    if (free_code) {
        MP_PLAT_FREE_EXEC(as->code_base, as->code_size);
    }
    m_del(size_t, as->label_offsets, as->max_num_labels);
}

void mp_asm_base_start_pass(mp_asm_base_t *as, int pass) {
    if (pass < MP_ASM_PASS_EMIT) {
        
        memset(as->label_offsets, -1, as->max_num_labels * sizeof(size_t));
    } else {
        
        MP_PLAT_ALLOC_EXEC(as->code_offset, (void **)&as->code_base, &as->code_size);
        assert(as->code_base != NULL);
    }
    as->pass = pass;
    as->suppress = false;
    as->code_offset = 0;
}





uint8_t *mp_asm_base_get_cur_to_write_bytes(void *as_in, size_t num_bytes_to_write) {
    mp_asm_base_t *as = as_in;
    uint8_t *c = NULL;
    if (as->suppress) {
        return c;
    }
    if (as->pass == MP_ASM_PASS_EMIT) {
        assert(as->code_offset + num_bytes_to_write <= as->code_size);
        c = as->code_base + as->code_offset;
    }
    as->code_offset += num_bytes_to_write;
    return c;
}

void mp_asm_base_label_assign(mp_asm_base_t *as, size_t label) {
    assert(label < as->max_num_labels);

    
    
    as->suppress = false;

    if (as->pass < MP_ASM_PASS_EMIT) {
        
        assert(as->label_offsets[label] == (size_t)-1);
        as->label_offsets[label] = as->code_offset;
    } else {
        
        assert(as->label_offsets[label] == as->code_offset);
    }
}


void mp_asm_base_align(mp_asm_base_t *as, unsigned int align) {
    as->code_offset = (as->code_offset + align - 1) & (~(align - 1));
}


void mp_asm_base_data(mp_asm_base_t *as, unsigned int bytesize, uintptr_t val) {
    uint8_t *c = mp_asm_base_get_cur_to_write_bytes(as, bytesize);
    if (c != NULL) {
        for (unsigned int i = 0; i < bytesize; i++) {
            *c++ = val;
            val >>= 8;
        }
    }
}

#endif 
