

#ifndef MICROPY_INCLUDED_PY_PROFILING_H
#define MICROPY_INCLUDED_PY_PROFILING_H

#include "py/emitglue.h"

#if MICROPY_PY_SYS_SETTRACE

#define mp_prof_is_executing MP_STATE_THREAD(prof_callback_is_executing)

typedef struct _mp_obj_code_t {
    
    mp_obj_base_t base;
    const mp_module_context_t *context;
    const mp_raw_code_t *rc;
    mp_obj_dict_t *dict_locals;
    mp_obj_t lnotab;
} mp_obj_code_t;

typedef struct _mp_obj_frame_t {
    mp_obj_base_t base;
    const mp_code_state_t *code_state;
    struct _mp_obj_frame_t *back;
    mp_obj_t callback;
    mp_obj_code_t *code;
    mp_uint_t lasti;
    mp_uint_t lineno;
    bool trace_opcodes;
} mp_obj_frame_t;

void mp_prof_extract_prelude(const byte *bytecode, mp_bytecode_prelude_t *prelude);

mp_obj_t mp_obj_new_code(const mp_module_context_t *mc, const mp_raw_code_t *rc);
mp_obj_t mp_obj_new_frame(const mp_code_state_t *code_state);


mp_obj_t mp_prof_settrace(mp_obj_t callback);

mp_obj_t mp_prof_frame_enter(mp_code_state_t *code_state);
mp_obj_t mp_prof_frame_update(const mp_code_state_t *code_state);


mp_obj_t mp_prof_instr_tick(mp_code_state_t *code_state, bool is_exception);



#define MICROPY_PROF_INSTR_DEBUG_PRINT_ENABLE 0
#if MICROPY_PROF_INSTR_DEBUG_PRINT_ENABLE
void mp_prof_print_instr(const byte *ip, mp_code_state_t *code_state);
#define MP_PROF_INSTR_DEBUG_PRINT(current_ip) mp_prof_print_instr((current_ip), code_state)
#else
#define MP_PROF_INSTR_DEBUG_PRINT(current_ip)
#endif

#endif 
#endif 
