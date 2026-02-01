 

#include <stdio.h>

#include "py/mphal.h"
#include "py/runtime.h"



void MICROPY_WRAP_MP_SCHED_EXCEPTION(mp_sched_exception)(mp_obj_t exc) {
    MP_STATE_MAIN_THREAD(mp_pending_exception) = exc;

    #if MICROPY_ENABLE_SCHEDULER && !MICROPY_PY_THREAD
    
    
    
    if (MP_STATE_VM(sched_state) == MP_SCHED_IDLE) {
        MP_STATE_VM(sched_state) = MP_SCHED_PENDING;
    }
    #endif
}

#if MICROPY_KBD_EXCEPTION

void MICROPY_WRAP_MP_SCHED_KEYBOARD_INTERRUPT(mp_sched_keyboard_interrupt)(void) {
    MP_STATE_VM(mp_kbd_exception).traceback_data = NULL;
    mp_sched_exception(MP_OBJ_FROM_PTR(&MP_STATE_VM(mp_kbd_exception)));
}
#endif

#if MICROPY_ENABLE_VM_ABORT
void MICROPY_WRAP_MP_SCHED_VM_ABORT(mp_sched_vm_abort)(void) {
    MP_STATE_VM(vm_abort) = true;
}
#endif

#if MICROPY_ENABLE_SCHEDULER

#define IDX_MASK(i) ((i) & (MICROPY_SCHEDULER_DEPTH - 1))



#define mp_sched_full() (mp_sched_num_pending() == MICROPY_SCHEDULER_DEPTH)

static inline bool mp_sched_empty(void) {
    MP_STATIC_ASSERT(MICROPY_SCHEDULER_DEPTH <= 255); 
    MP_STATIC_ASSERT((IDX_MASK(MICROPY_SCHEDULER_DEPTH) == 0)); 

    return mp_sched_num_pending() == 0;
}

static inline void mp_sched_run_pending(void) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    if (MP_STATE_VM(sched_state) != MP_SCHED_PENDING) {
        
        
        MICROPY_END_ATOMIC_SECTION(atomic_state);
        return;
    }

    
    
    MP_STATE_VM(sched_state) = MP_SCHED_LOCKED;

    #if MICROPY_SCHEDULER_STATIC_NODES
    
    while (MP_STATE_VM(sched_head) != NULL) {
        mp_sched_node_t *node = MP_STATE_VM(sched_head);
        MP_STATE_VM(sched_head) = node->next;
        if (MP_STATE_VM(sched_head) == NULL) {
            MP_STATE_VM(sched_tail) = NULL;
        }
        mp_sched_callback_t callback = node->callback;
        node->callback = NULL;
        MICROPY_END_ATOMIC_SECTION(atomic_state);
        callback(node);
        atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    }
    #endif

    
    if (!mp_sched_empty()) {
        mp_sched_item_t item = MP_STATE_VM(sched_queue)[MP_STATE_VM(sched_idx)];
        MP_STATE_VM(sched_idx) = IDX_MASK(MP_STATE_VM(sched_idx) + 1);
        --MP_STATE_VM(sched_len);
        MICROPY_END_ATOMIC_SECTION(atomic_state);
        mp_call_function_1_protected(item.func, item.arg);
    } else {
        MICROPY_END_ATOMIC_SECTION(atomic_state);
    }

    
    
    mp_sched_unlock();
}




void mp_sched_lock(void) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    if (MP_STATE_VM(sched_state) < 0) {
        
        --MP_STATE_VM(sched_state);
    } else {
        
        MP_STATE_VM(sched_state) = MP_SCHED_LOCKED;
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
}

void mp_sched_unlock(void) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    assert(MP_STATE_VM(sched_state) < 0);
    if (++MP_STATE_VM(sched_state) == 0) {
        
        
        if (
            #if !MICROPY_PY_THREAD
            
            MP_STATE_THREAD(mp_pending_exception) != MP_OBJ_NULL ||
            #endif
            #if MICROPY_SCHEDULER_STATIC_NODES
            MP_STATE_VM(sched_head) != NULL ||
            #endif
            mp_sched_num_pending()) {
            MP_STATE_VM(sched_state) = MP_SCHED_PENDING;
        } else {
            MP_STATE_VM(sched_state) = MP_SCHED_IDLE;
        }
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
}

bool MICROPY_WRAP_MP_SCHED_SCHEDULE(mp_sched_schedule)(mp_obj_t function, mp_obj_t arg) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    bool ret;
    if (!mp_sched_full()) {
        if (MP_STATE_VM(sched_state) == MP_SCHED_IDLE) {
            MP_STATE_VM(sched_state) = MP_SCHED_PENDING;
        }
        uint8_t iput = IDX_MASK(MP_STATE_VM(sched_idx) + MP_STATE_VM(sched_len)++);
        MP_STATE_VM(sched_queue)[iput].func = function;
        MP_STATE_VM(sched_queue)[iput].arg = arg;
        MICROPY_SCHED_HOOK_SCHEDULED;
        ret = true;
    } else {
        
        ret = false;
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
    return ret;
}

#if MICROPY_SCHEDULER_STATIC_NODES
bool mp_sched_schedule_node(mp_sched_node_t *node, mp_sched_callback_t callback) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    bool ret;
    if (node->callback == NULL) {
        if (MP_STATE_VM(sched_state) == MP_SCHED_IDLE) {
            MP_STATE_VM(sched_state) = MP_SCHED_PENDING;
        }
        node->callback = callback;
        node->next = NULL;
        if (MP_STATE_VM(sched_tail) == NULL) {
            MP_STATE_VM(sched_head) = node;
        } else {
            MP_STATE_VM(sched_tail)->next = node;
        }
        MP_STATE_VM(sched_tail) = node;
        MICROPY_SCHED_HOOK_SCHEDULED;
        ret = true;
    } else {
        
        ret = false;
    }
    MICROPY_END_ATOMIC_SECTION(atomic_state);
    return ret;
}
#endif

MP_REGISTER_ROOT_POINTER(mp_sched_item_t sched_queue[MICROPY_SCHEDULER_DEPTH]);

#endif 



void mp_handle_pending(bool raise_exc) {
    
    #if MICROPY_ENABLE_VM_ABORT
    if (MP_STATE_VM(vm_abort) && mp_thread_is_main_thread()) {
        MP_STATE_VM(vm_abort) = false;
        if (raise_exc && nlr_get_abort() != NULL) {
            nlr_jump_abort();
        }
    }
    #endif

    
    if (MP_STATE_THREAD(mp_pending_exception) != MP_OBJ_NULL) {
        mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
        mp_obj_t obj = MP_STATE_THREAD(mp_pending_exception);
        if (obj != MP_OBJ_NULL) {
            MP_STATE_THREAD(mp_pending_exception) = MP_OBJ_NULL;
            if (raise_exc) {
                MICROPY_END_ATOMIC_SECTION(atomic_state);
                nlr_raise(obj);
            }
        }
        MICROPY_END_ATOMIC_SECTION(atomic_state);
    }

    
    #if MICROPY_ENABLE_SCHEDULER
    if (MP_STATE_VM(sched_state) == MP_SCHED_PENDING) {
        mp_sched_run_pending();
    }
    #endif
}


void mp_event_handle_nowait(void) {
    #if defined(MICROPY_EVENT_POLL_HOOK_FAST) && !MICROPY_PREVIEW_VERSION_2
    
    MICROPY_EVENT_POLL_HOOK_FAST
    #else
    
    MICROPY_INTERNAL_EVENT_HOOK;
    mp_handle_pending(true);
    #endif
}



void mp_event_wait_indefinite(void) {
    #if defined(MICROPY_EVENT_POLL_HOOK) && !MICROPY_PREVIEW_VERSION_2
    
    MICROPY_EVENT_POLL_HOOK
    #else
    mp_event_handle_nowait();
    MICROPY_INTERNAL_WFE(-1);
    #endif
}



void mp_event_wait_ms(mp_uint_t timeout_ms) {
    #if defined(MICROPY_EVENT_POLL_HOOK) && !MICROPY_PREVIEW_VERSION_2
    
    MICROPY_EVENT_POLL_HOOK
    #else
    mp_event_handle_nowait();
    MICROPY_INTERNAL_WFE(timeout_ms);
    #endif
}
