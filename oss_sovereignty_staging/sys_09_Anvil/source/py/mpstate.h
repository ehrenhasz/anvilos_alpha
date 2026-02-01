 
#ifndef MICROPY_INCLUDED_PY_MPSTATE_H
#define MICROPY_INCLUDED_PY_MPSTATE_H

#include <stdint.h>

#include "py/mpconfig.h"
#include "py/mpthread.h"
#include "py/misc.h"
#include "py/nlr.h"
#include "py/obj.h"
#include "py/objlist.h"
#include "py/objexcept.h"





#if MICROPY_PY_SYS_ATTR_DELEGATION

enum {
    #if MICROPY_PY_SYS_PATH
    MP_SYS_MUTABLE_PATH,
    #endif
    #if MICROPY_PY_SYS_PS1_PS2
    MP_SYS_MUTABLE_PS1,
    MP_SYS_MUTABLE_PS2,
    #endif
    #if MICROPY_PY_SYS_TRACEBACKLIMIT
    MP_SYS_MUTABLE_TRACEBACKLIMIT,
    #endif
    MP_SYS_MUTABLE_NUM,
};
#endif 


#if MICROPY_DYNAMIC_COMPILER
typedef struct mp_dynamic_compiler_t {
    uint8_t small_int_bits; 
    uint8_t native_arch;
    uint8_t nlr_buf_num_regs;
} mp_dynamic_compiler_t;
extern mp_dynamic_compiler_t mp_dynamic_compiler;
#endif


#define MP_SCHED_IDLE (1)
#define MP_SCHED_LOCKED (-1)
#define MP_SCHED_PENDING (0) 

typedef struct _mp_sched_item_t {
    mp_obj_t func;
    mp_obj_t arg;
} mp_sched_item_t;



typedef struct _mp_state_mem_area_t {
    #if MICROPY_GC_SPLIT_HEAP
    struct _mp_state_mem_area_t *next;
    #endif

    byte *gc_alloc_table_start;
    size_t gc_alloc_table_byte_len;
    #if MICROPY_ENABLE_FINALISER
    byte *gc_finaliser_table_start;
    #endif
    byte *gc_pool_start;
    byte *gc_pool_end;

    size_t gc_last_free_atb_index;
    size_t gc_last_used_block; 
} mp_state_mem_area_t;


typedef struct _mp_state_mem_t {
    #if MICROPY_MEM_STATS
    size_t total_bytes_allocated;
    size_t current_bytes_allocated;
    size_t peak_bytes_allocated;
    #endif

    mp_state_mem_area_t area;

    int gc_stack_overflow;
    MICROPY_GC_STACK_ENTRY_TYPE gc_block_stack[MICROPY_ALLOC_GC_STACK_SIZE];
    #if MICROPY_GC_SPLIT_HEAP
    
    mp_state_mem_area_t *gc_area_stack[MICROPY_ALLOC_GC_STACK_SIZE];
    #endif

    
    
    
    uint16_t gc_auto_collect_enabled;

    #if MICROPY_GC_ALLOC_THRESHOLD
    size_t gc_alloc_amount;
    size_t gc_alloc_threshold;
    #endif

    #if MICROPY_GC_SPLIT_HEAP
    mp_state_mem_area_t *gc_last_free_area;
    #endif

    #if MICROPY_PY_GC_COLLECT_RETVAL
    size_t gc_collected;
    #endif

    #if MICROPY_PY_THREAD && !MICROPY_PY_THREAD_GIL
    
    mp_thread_mutex_t gc_mutex;
    #endif
} mp_state_mem_t;



typedef struct _mp_state_vm_t {
    
    
    
    
    
    

    qstr_pool_t *last_pool;

    #if MICROPY_TRACKED_ALLOC
    struct _m_tracked_node_t *m_tracked_head;
    #endif

    
    mp_obj_exception_t mp_emergency_exception_obj;

    
    #if MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF
    #if MICROPY_EMERGENCY_EXCEPTION_BUF_SIZE > 0
    
    mp_obj_t mp_emergency_exception_buf[MICROPY_EMERGENCY_EXCEPTION_BUF_SIZE / sizeof(mp_obj_t)];
    #else
    
    byte *mp_emergency_exception_buf;
    #endif
    #endif

    #if MICROPY_KBD_EXCEPTION
    
    mp_obj_exception_t mp_kbd_exception;
    #endif

    
    mp_obj_dict_t mp_loaded_modules_dict;

    
    mp_obj_dict_t dict_main;

    
    #if MICROPY_CAN_OVERRIDE_BUILTINS
    mp_obj_dict_t *mp_module_builtins_override_dict;
    #endif

    
    #ifndef NO_QSTR
    
    
    #include "genhdr/root_pointers.h"
    #endif

    
    
    

    
    
    char *qstr_last_chunk;
    size_t qstr_last_alloc;
    size_t qstr_last_used;

    #if MICROPY_PY_THREAD && !MICROPY_PY_THREAD_GIL
    
    mp_thread_mutex_t qstr_mutex;
    #endif

    #if MICROPY_ENABLE_COMPILER
    mp_uint_t mp_optimise_value;
    #if MICROPY_EMIT_NATIVE
    uint8_t default_emit_opt; 
    #endif
    #endif

    
    #if MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF && MICROPY_EMERGENCY_EXCEPTION_BUF_SIZE == 0
    mp_int_t mp_emergency_exception_buf_size;
    #endif

    #if MICROPY_ENABLE_SCHEDULER
    volatile int16_t sched_state;

    #if MICROPY_SCHEDULER_STATIC_NODES
    
    
    
    struct _mp_sched_node_t *sched_head;
    struct _mp_sched_node_t *sched_tail;
    #endif

    
    uint8_t sched_len;
    uint8_t sched_idx;
    #endif

    #if MICROPY_ENABLE_VM_ABORT
    bool vm_abort;
    nlr_buf_t *nlr_abort;
    #endif

    #if MICROPY_PY_THREAD_GIL
    
    mp_thread_mutex_t gil_mutex;
    #endif

    #if MICROPY_OPT_MAP_LOOKUP_CACHE
    
    uint8_t map_lookup_cache[MICROPY_OPT_MAP_LOOKUP_CACHE_SIZE];
    #endif
} mp_state_vm_t;





typedef struct _mp_state_thread_t {
    
    char *stack_top;

    #if MICROPY_STACK_CHECK
    size_t stack_limit;
    #endif

    #if MICROPY_ENABLE_PYSTACK
    uint8_t *pystack_start;
    uint8_t *pystack_end;
    uint8_t *pystack_cur;
    #endif

    
    uint16_t gc_lock_depth;

    
    
    
    
    

    mp_obj_dict_t *dict_locals;
    mp_obj_dict_t *dict_globals;

    nlr_buf_t *nlr_top;
    nlr_jump_callback_node_t *nlr_jump_callback_top;

    
    volatile mp_obj_t mp_pending_exception;

    
    mp_obj_t stop_iteration_arg;

    #if MICROPY_PY_SYS_SETTRACE
    mp_obj_t prof_trace_callback;
    bool prof_callback_is_executing;
    struct _mp_code_state_t *current_code_state;
    #endif
} mp_state_thread_t;



typedef struct _mp_state_ctx_t {
    mp_state_thread_t thread;
    mp_state_vm_t vm;
    mp_state_mem_t mem;
} mp_state_ctx_t;

extern mp_state_ctx_t mp_state_ctx;

#define MP_STATE_VM(x) (mp_state_ctx.vm.x)
#define MP_STATE_MEM(x) (mp_state_ctx.mem.x)
#define MP_STATE_MAIN_THREAD(x) (mp_state_ctx.thread.x)

#if MICROPY_PY_THREAD
#define MP_STATE_THREAD(x) (mp_thread_get_state()->x)
#define mp_thread_is_main_thread() (mp_thread_get_state() == &mp_state_ctx.thread)
#else
#define MP_STATE_THREAD(x)  MP_STATE_MAIN_THREAD(x)
#define mp_thread_is_main_thread() (true)
#endif

#endif 
