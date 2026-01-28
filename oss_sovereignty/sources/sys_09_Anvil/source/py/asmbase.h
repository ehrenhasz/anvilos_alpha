
#ifndef MICROPY_INCLUDED_PY_ASMBASE_H
#define MICROPY_INCLUDED_PY_ASMBASE_H

#include <stdint.h>
#include <stdbool.h>

#define MP_ASM_PASS_COMPUTE (1)
#define MP_ASM_PASS_EMIT    (2)

typedef struct _mp_asm_base_t {
    uint8_t pass;

    
    
    bool suppress;

    size_t code_offset;
    size_t code_size;
    uint8_t *code_base;

    size_t max_num_labels;
    size_t *label_offsets;
} mp_asm_base_t;

void mp_asm_base_init(mp_asm_base_t *as, size_t max_num_labels);
void mp_asm_base_deinit(mp_asm_base_t *as, bool free_code);
void mp_asm_base_start_pass(mp_asm_base_t *as, int pass);
uint8_t *mp_asm_base_get_cur_to_write_bytes(void *as, size_t num_bytes_to_write);
void mp_asm_base_label_assign(mp_asm_base_t *as, size_t label);
void mp_asm_base_align(mp_asm_base_t *as, unsigned int align);
void mp_asm_base_data(mp_asm_base_t *as, unsigned int bytesize, uintptr_t val);

static inline void mp_asm_base_suppress_code(mp_asm_base_t *as) {
    as->suppress = true;
}

static inline size_t mp_asm_base_get_code_pos(mp_asm_base_t *as) {
    return as->code_offset;
}

static inline size_t mp_asm_base_get_code_size(mp_asm_base_t *as) {
    return as->code_size;
}

static inline void *mp_asm_base_get_code(mp_asm_base_t *as) {
    #if defined(MP_PLAT_COMMIT_EXEC)
    return MP_PLAT_COMMIT_EXEC(as->code_base, as->code_size, NULL);
    #else
    return as->code_base;
    #endif
}

#endif 
