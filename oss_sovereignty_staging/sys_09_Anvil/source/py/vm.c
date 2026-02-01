 

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "py/emitglue.h"
#include "py/objtype.h"
#include "py/objfun.h"
#include "py/runtime.h"
#include "py/bc0.h"
#include "py/profile.h"



#if 0
#if MICROPY_PY_THREAD
#define TRACE_PREFIX mp_printf(&mp_plat_print, "ts=%p sp=%d ", mp_thread_get_state(), (int)(sp - &code_state->state[0] + 1))
#else
#define TRACE_PREFIX mp_printf(&mp_plat_print, "sp=%d ", (int)(sp - &code_state->state[0] + 1))
#endif
#define TRACE(ip) TRACE_PREFIX; mp_bytecode_print2(&mp_plat_print, ip, 1, code_state->fun_bc->child_table, &code_state->fun_bc->context->constants);
#else
#define TRACE(ip)
#endif








#define DECODE_UINT \
    mp_uint_t unum = 0; \
    do { \
        unum = (unum << 7) + (*ip & 0x7f); \
    } while ((*ip++ & 0x80) != 0)

#define DECODE_ULABEL \
    size_t ulab; \
    do { \
        if (ip[0] & 0x80) { \
            ulab = ((ip[0] & 0x7f) | (ip[1] << 7)); \
            ip += 2; \
        } else { \
            ulab = ip[0]; \
            ip += 1; \
        } \
    } while (0)

#define DECODE_SLABEL \
    size_t slab; \
    do { \
        if (ip[0] & 0x80) { \
            slab = ((ip[0] & 0x7f) | (ip[1] << 7)) - 0x4000; \
            ip += 2; \
        } else { \
            slab = ip[0] - 0x40; \
            ip += 1; \
        } \
    } while (0)

#if MICROPY_EMIT_BYTECODE_USES_QSTR_TABLE

#define DECODE_QSTR \
    DECODE_UINT; \
    qstr qst = qstr_table[unum]

#else

#define DECODE_QSTR \
    DECODE_UINT; \
    qstr qst = unum;

#endif

#define DECODE_PTR \
    DECODE_UINT; \
    void *ptr = (void *)(uintptr_t)code_state->fun_bc->child_table[unum]

#define DECODE_OBJ \
    DECODE_UINT; \
    mp_obj_t obj = (mp_obj_t)code_state->fun_bc->context->constants.obj_table[unum]

#define PUSH(val) *++sp = (val)
#define POP() (*sp--)
#define TOP() (*sp)
#define SET_TOP(val) *sp = (val)

#if MICROPY_PY_SYS_EXC_INFO
#define CLEAR_SYS_EXC_INFO() MP_STATE_VM(cur_exception) = NULL;
#else
#define CLEAR_SYS_EXC_INFO()
#endif

#define PUSH_EXC_BLOCK(with_or_finally) do { \
    DECODE_ULABEL;   \
    ++exc_sp; \
    exc_sp->handler = ip + ulab; \
    exc_sp->val_sp = MP_TAGPTR_MAKE(sp, ((with_or_finally) << 1)); \
    exc_sp->prev_exc = NULL; \
} while (0)

#define POP_EXC_BLOCK() \
    exc_sp--;   \
    CLEAR_SYS_EXC_INFO()  

#define CANCEL_ACTIVE_FINALLY(sp) do { \
    if (mp_obj_is_small_int(sp[-1])) { \
          \
          \
        sp[-2] = sp[0]; \
        sp -= 2; \
    } else { \
        assert(sp[-1] == mp_const_none || mp_obj_is_exception_instance(sp[-1])); \
          \
          \
        sp[-1] = sp[0]; \
        --sp; \
    } \
} while (0)

#if MICROPY_PY_SYS_SETTRACE

#define FRAME_SETUP() do { \
    assert(code_state != code_state->prev_state); \
    MP_STATE_THREAD(current_code_state) = code_state; \
    assert(code_state != code_state->prev_state); \
} while(0)

#define FRAME_ENTER() do { \
    assert(code_state != code_state->prev_state); \
    code_state->prev_state = MP_STATE_THREAD(current_code_state); \
    assert(code_state != code_state->prev_state); \
    if (!mp_prof_is_executing) { \
        mp_prof_frame_enter(code_state); \
    } \
} while(0)

#define FRAME_LEAVE() do { \
    assert(code_state != code_state->prev_state); \
    MP_STATE_THREAD(current_code_state) = code_state->prev_state; \
    assert(code_state != code_state->prev_state); \
} while(0)

#define FRAME_UPDATE() do { \
    assert(MP_STATE_THREAD(current_code_state) == code_state); \
    if (!mp_prof_is_executing) { \
        code_state->frame = MP_OBJ_TO_PTR(mp_prof_frame_update(code_state)); \
    } \
} while(0)

#define TRACE_TICK(current_ip, current_sp, is_exception) do { \
    assert(code_state != code_state->prev_state); \
    assert(MP_STATE_THREAD(current_code_state) == code_state); \
    if (!mp_prof_is_executing && code_state->frame && MP_STATE_THREAD(prof_trace_callback)) { \
        MP_PROF_INSTR_DEBUG_PRINT(code_state->ip); \
    } \
    if (!mp_prof_is_executing && code_state->frame && code_state->frame->callback) { \
        mp_prof_instr_tick(code_state, is_exception); \
    } \
} while(0)

#else 
#define FRAME_SETUP()
#define FRAME_ENTER()
#define FRAME_LEAVE()
#define FRAME_UPDATE()
#define TRACE_TICK(current_ip, current_sp, is_exception)
#endif 







mp_vm_return_kind_t MICROPY_WRAP_MP_EXECUTE_BYTECODE(mp_execute_bytecode)(mp_code_state_t *code_state, volatile mp_obj_t inject_exc) {

#define SELECTIVE_EXC_IP (0)









#if SELECTIVE_EXC_IP


#define MARK_EXC_IP_SELECTIVE() { code_state->ip = ip; }

#define MARK_EXC_IP_GLOBAL()
#else
#define MARK_EXC_IP_SELECTIVE()


#define MARK_EXC_IP_GLOBAL() { code_state->ip = ip; }
#endif

#if MICROPY_OPT_COMPUTED_GOTO
    #include "py/vmentrytable.h"
    #define DISPATCH() do { \
        TRACE(ip); \
        MARK_EXC_IP_GLOBAL(); \
        TRACE_TICK(ip, sp, false); \
        goto *entry_table[*ip++]; \
    } while (0)
    #define DISPATCH_WITH_PEND_EXC_CHECK() goto pending_exception_check
    #define ENTRY(op) entry_##op
    #define ENTRY_DEFAULT entry_default
#else
    #define DISPATCH() goto dispatch_loop
    #define DISPATCH_WITH_PEND_EXC_CHECK() goto pending_exception_check
    #define ENTRY(op) case op
    #define ENTRY_DEFAULT default
#endif

    
    
    
    
    #define RAISE(o) do { nlr_pop(); nlr.ret_val = MP_OBJ_TO_PTR(o); goto exception_handler; } while (0)

#if MICROPY_STACKLESS
run_code_state: ;
#endif
FRAME_ENTER();

#if MICROPY_STACKLESS
run_code_state_from_return: ;
#endif
FRAME_SETUP();

    
    mp_obj_t *   fastn;
    mp_exc_stack_t *   exc_stack;
    {
        size_t n_state = code_state->n_state;
        fastn = &code_state->state[n_state - 1];
        exc_stack = (mp_exc_stack_t*)(code_state->state + n_state);
    }

    
    mp_exc_stack_t *volatile exc_sp = MP_CODE_STATE_EXC_SP_IDX_TO_PTR(exc_stack, code_state->exc_sp_idx); 

    #if MICROPY_PY_THREAD_GIL && MICROPY_PY_THREAD_GIL_VM_DIVISOR
    
    
    volatile int gil_divisor = MICROPY_PY_THREAD_GIL_VM_DIVISOR;
    #endif

    
    for (;;) {
        nlr_buf_t nlr;
outer_dispatch_loop:
        if (nlr_push(&nlr) == 0) {
            
            const byte *ip = code_state->ip;
            mp_obj_t *sp = code_state->sp;
            #if MICROPY_EMIT_BYTECODE_USES_QSTR_TABLE
            const qstr_short_t *qstr_table = code_state->fun_bc->context->constants.qstr_table;
            #endif
            mp_obj_t obj_shared;
            MICROPY_VM_HOOK_INIT

            
            
            
            
            
            if (inject_exc != MP_OBJ_NULL && *ip != MP_BC_YIELD_FROM) {
                mp_obj_t exc = inject_exc;
                inject_exc = MP_OBJ_NULL;
                exc = mp_make_raise_obj(exc);
                RAISE(exc);
            }

            
            for (;;) {
dispatch_loop:
                #if MICROPY_OPT_COMPUTED_GOTO
                DISPATCH();
                #else
                TRACE(ip);
                MARK_EXC_IP_GLOBAL();
                TRACE_TICK(ip, sp, false);
                switch (*ip++) {
                #endif

                ENTRY(MP_BC_LOAD_CONST_FALSE):
                    PUSH(mp_const_false);
                    DISPATCH();

                ENTRY(MP_BC_LOAD_CONST_NONE):
                    PUSH(mp_const_none);
                    DISPATCH();

                ENTRY(MP_BC_LOAD_CONST_TRUE):
                    PUSH(mp_const_true);
                    DISPATCH();

                ENTRY(MP_BC_LOAD_CONST_SMALL_INT): {
                    mp_uint_t num = 0;
                    if ((ip[0] & 0x40) != 0) {
                        
                        num--;
                    }
                    do {
                        num = (num << 7) | (*ip & 0x7f);
                    } while ((*ip++ & 0x80) != 0);
                    PUSH(MP_OBJ_NEW_SMALL_INT(num));
                    DISPATCH();
                }

                ENTRY(MP_BC_LOAD_CONST_STRING): {
                    DECODE_QSTR;
                    PUSH(MP_OBJ_NEW_QSTR(qst));
                    DISPATCH();
                }

                ENTRY(MP_BC_LOAD_CONST_OBJ): {
                    DECODE_OBJ;
                    PUSH(obj);
                    DISPATCH();
                }

                ENTRY(MP_BC_LOAD_NULL):
                    PUSH(MP_OBJ_NULL);
                    DISPATCH();

                ENTRY(MP_BC_LOAD_FAST_N): {
                    DECODE_UINT;
                    obj_shared = fastn[-unum];
                    load_check:
                    if (obj_shared == MP_OBJ_NULL) {
                        local_name_error: {
                            MARK_EXC_IP_SELECTIVE();
                            mp_obj_t obj = mp_obj_new_exception_msg(&mp_type_NameError, MP_ERROR_TEXT("local variable referenced before assignment"));
                            RAISE(obj);
                        }
                    }
                    PUSH(obj_shared);
                    DISPATCH();
                }

                ENTRY(MP_BC_LOAD_DEREF): {
                    DECODE_UINT;
                    obj_shared = mp_obj_cell_get(fastn[-unum]);
                    goto load_check;
                }

                ENTRY(MP_BC_LOAD_NAME): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_QSTR;
                    PUSH(mp_load_name(qst));
                    DISPATCH();
                }

                ENTRY(MP_BC_LOAD_GLOBAL): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_QSTR;
                    PUSH(mp_load_global(qst));
                    DISPATCH();
                }

                ENTRY(MP_BC_LOAD_ATTR): {
                    FRAME_UPDATE();
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_QSTR;
                    mp_obj_t top = TOP();
                    mp_obj_t obj;
                    #if MICROPY_OPT_LOAD_ATTR_FAST_PATH
                    
                    
                    
                    
                    mp_map_elem_t *elem = NULL;
                    if (mp_obj_is_instance_type(mp_obj_get_type(top))) {
                        mp_obj_instance_t *self = MP_OBJ_TO_PTR(top);
                        elem = mp_map_lookup(&self->members, MP_OBJ_NEW_QSTR(qst), MP_MAP_LOOKUP);
                    }
                    if (elem) {
                        obj = elem->value;
                    } else
                    #endif
                    {
                        obj = mp_load_attr(top, qst);
                    }
                    SET_TOP(obj);
                    DISPATCH();
                }

                ENTRY(MP_BC_LOAD_METHOD): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_QSTR;
                    mp_load_method(*sp, qst, sp);
                    sp += 1;
                    DISPATCH();
                }

                ENTRY(MP_BC_LOAD_SUPER_METHOD): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_QSTR;
                    sp -= 1;
                    mp_load_super_method(qst, sp - 1);
                    DISPATCH();
                }

                ENTRY(MP_BC_LOAD_BUILD_CLASS):
                    MARK_EXC_IP_SELECTIVE();
                    PUSH(mp_load_build_class());
                    DISPATCH();

                ENTRY(MP_BC_LOAD_SUBSCR): {
                    MARK_EXC_IP_SELECTIVE();
                    mp_obj_t index = POP();
                    SET_TOP(mp_obj_subscr(TOP(), index, MP_OBJ_SENTINEL));
                    DISPATCH();
                }

                ENTRY(MP_BC_STORE_FAST_N): {
                    DECODE_UINT;
                    fastn[-unum] = POP();
                    DISPATCH();
                }

                ENTRY(MP_BC_STORE_DEREF): {
                    DECODE_UINT;
                    mp_obj_cell_set(fastn[-unum], POP());
                    DISPATCH();
                }

                ENTRY(MP_BC_STORE_NAME): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_QSTR;
                    mp_store_name(qst, POP());
                    DISPATCH();
                }

                ENTRY(MP_BC_STORE_GLOBAL): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_QSTR;
                    mp_store_global(qst, POP());
                    DISPATCH();
                }

                ENTRY(MP_BC_STORE_ATTR): {
                    FRAME_UPDATE();
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_QSTR;
                    mp_store_attr(sp[0], qst, sp[-1]);
                    sp -= 2;
                    DISPATCH();
                }

                ENTRY(MP_BC_STORE_SUBSCR):
                    MARK_EXC_IP_SELECTIVE();
                    mp_obj_subscr(sp[-1], sp[0], sp[-2]);
                    sp -= 3;
                    DISPATCH();

                ENTRY(MP_BC_DELETE_FAST): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_UINT;
                    if (fastn[-unum] == MP_OBJ_NULL) {
                        goto local_name_error;
                    }
                    fastn[-unum] = MP_OBJ_NULL;
                    DISPATCH();
                }

                ENTRY(MP_BC_DELETE_DEREF): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_UINT;
                    if (mp_obj_cell_get(fastn[-unum]) == MP_OBJ_NULL) {
                        goto local_name_error;
                    }
                    mp_obj_cell_set(fastn[-unum], MP_OBJ_NULL);
                    DISPATCH();
                }

                ENTRY(MP_BC_DELETE_NAME): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_QSTR;
                    mp_delete_name(qst);
                    DISPATCH();
                }

                ENTRY(MP_BC_DELETE_GLOBAL): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_QSTR;
                    mp_delete_global(qst);
                    DISPATCH();
                }

                ENTRY(MP_BC_DUP_TOP): {
                    mp_obj_t top = TOP();
                    PUSH(top);
                    DISPATCH();
                }

                ENTRY(MP_BC_DUP_TOP_TWO):
                    sp += 2;
                    sp[0] = sp[-2];
                    sp[-1] = sp[-3];
                    DISPATCH();

                ENTRY(MP_BC_POP_TOP):
                    sp -= 1;
                    DISPATCH();

                ENTRY(MP_BC_ROT_TWO): {
                    mp_obj_t top = sp[0];
                    sp[0] = sp[-1];
                    sp[-1] = top;
                    DISPATCH();
                }

                ENTRY(MP_BC_ROT_THREE): {
                    mp_obj_t top = sp[0];
                    sp[0] = sp[-1];
                    sp[-1] = sp[-2];
                    sp[-2] = top;
                    DISPATCH();
                }

                ENTRY(MP_BC_JUMP): {
                    DECODE_SLABEL;
                    ip += slab;
                    DISPATCH_WITH_PEND_EXC_CHECK();
                }

                ENTRY(MP_BC_POP_JUMP_IF_TRUE): {
                    DECODE_SLABEL;
                    if (mp_obj_is_true(POP())) {
                        ip += slab;
                    }
                    DISPATCH_WITH_PEND_EXC_CHECK();
                }

                ENTRY(MP_BC_POP_JUMP_IF_FALSE): {
                    DECODE_SLABEL;
                    if (!mp_obj_is_true(POP())) {
                        ip += slab;
                    }
                    DISPATCH_WITH_PEND_EXC_CHECK();
                }

                ENTRY(MP_BC_JUMP_IF_TRUE_OR_POP): {
                    DECODE_ULABEL;
                    if (mp_obj_is_true(TOP())) {
                        ip += ulab;
                    } else {
                        sp--;
                    }
                    DISPATCH_WITH_PEND_EXC_CHECK();
                }

                ENTRY(MP_BC_JUMP_IF_FALSE_OR_POP): {
                    DECODE_ULABEL;
                    if (mp_obj_is_true(TOP())) {
                        sp--;
                    } else {
                        ip += ulab;
                    }
                    DISPATCH_WITH_PEND_EXC_CHECK();
                }

                ENTRY(MP_BC_SETUP_WITH): {
                    MARK_EXC_IP_SELECTIVE();
                    
                    mp_obj_t obj = TOP();
                    mp_load_method(obj, MP_QSTR___exit__, sp);
                    mp_load_method(obj, MP_QSTR___enter__, sp + 2);
                    mp_obj_t ret = mp_call_method_n_kw(0, 0, sp + 2);
                    sp += 1;
                    PUSH_EXC_BLOCK(1);
                    PUSH(ret);
                    
                    DISPATCH();
                }

                ENTRY(MP_BC_WITH_CLEANUP): {
                    MARK_EXC_IP_SELECTIVE();
                    
                    
                    
                    
                    
                    
                    if (TOP() == mp_const_none) {
                        
                        sp[1] = mp_const_none;
                        sp[2] = mp_const_none;
                        sp -= 2;
                        mp_call_method_n_kw(3, 0, sp);
                        SET_TOP(mp_const_none);
                    } else if (mp_obj_is_small_int(TOP())) {
                        
                        
                        
                        
                        mp_obj_t data = sp[-1];
                        mp_obj_t cause = sp[0];
                        sp[-1] = mp_const_none;
                        sp[0] = mp_const_none;
                        sp[1] = mp_const_none;
                        mp_call_method_n_kw(3, 0, sp - 3);
                        sp[-3] = data;
                        sp[-2] = cause;
                        sp -= 2; 
                    } else {
                        assert(mp_obj_is_exception_instance(TOP()));
                        
                        
                        sp[1] = sp[0];
                        sp[0] = MP_OBJ_FROM_PTR(mp_obj_get_type(sp[0]));
                        sp[2] = mp_const_none;
                        sp -= 2;
                        mp_obj_t ret_value = mp_call_method_n_kw(3, 0, sp);
                        if (mp_obj_is_true(ret_value)) {
                            
                            
                            
                            
                            SET_TOP(mp_const_none);
                        } else {
                            
                            
                            sp[0] = sp[3];
                        }
                    }
                    DISPATCH();
                }

                ENTRY(MP_BC_UNWIND_JUMP): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_SLABEL;
                    PUSH((mp_obj_t)(mp_uint_t)(uintptr_t)(ip + slab)); 
                    PUSH((mp_obj_t)(mp_uint_t)(*ip)); 
unwind_jump:;
                    mp_uint_t unum = (mp_uint_t)POP(); 
                    while ((unum & 0x7f) > 0) {
                        unum -= 1;
                        assert(exc_sp >= exc_stack);

                        if (MP_TAGPTR_TAG1(exc_sp->val_sp)) {
                            if (exc_sp->handler >= ip) {
                                
                                
                                
                                
                                
                                assert(&sp[-1] == MP_TAGPTR_PTR(exc_sp->val_sp));
                                
                                
                                
                                
                                
                                
                                PUSH(MP_OBJ_NEW_SMALL_INT(unum));
                                ip = exc_sp->handler;
                                goto dispatch_loop;
                            } else {
                                
                                CANCEL_ACTIVE_FINALLY(sp);
                            }
                        }
                        POP_EXC_BLOCK();
                    }
                    ip = (const byte*)MP_OBJ_TO_PTR(POP()); 
                    if (unum != 0) {
                        
                        sp -= MP_OBJ_ITER_BUF_NSLOTS;
                    }
                    DISPATCH_WITH_PEND_EXC_CHECK();
                }

                ENTRY(MP_BC_SETUP_EXCEPT):
                ENTRY(MP_BC_SETUP_FINALLY): {
                    MARK_EXC_IP_SELECTIVE();
                    #if SELECTIVE_EXC_IP
                    PUSH_EXC_BLOCK((code_state->ip[-1] == MP_BC_SETUP_FINALLY) ? 1 : 0);
                    #else
                    PUSH_EXC_BLOCK((code_state->ip[0] == MP_BC_SETUP_FINALLY) ? 1 : 0);
                    #endif
                    DISPATCH();
                }

                ENTRY(MP_BC_END_FINALLY):
                    MARK_EXC_IP_SELECTIVE();
                    
                    
                    
                    assert(exc_sp >= exc_stack);
                    POP_EXC_BLOCK();
                    if (TOP() == mp_const_none) {
                        sp--;
                    } else if (mp_obj_is_small_int(TOP())) {
                        
                        
                        mp_int_t cause = MP_OBJ_SMALL_INT_VALUE(POP());
                        if (cause < 0) {
                            
                            goto unwind_return;
                        } else {
                            
                            
                            PUSH((mp_obj_t)cause);
                            goto unwind_jump;
                        }
                    } else {
                        assert(mp_obj_is_exception_instance(TOP()));
                        RAISE(TOP());
                    }
                    DISPATCH();

                ENTRY(MP_BC_GET_ITER):
                    MARK_EXC_IP_SELECTIVE();
                    SET_TOP(mp_getiter(TOP(), NULL));
                    DISPATCH();

                
                
                
                
                ENTRY(MP_BC_GET_ITER_STACK): {
                    MARK_EXC_IP_SELECTIVE();
                    mp_obj_t obj = TOP();
                    mp_obj_iter_buf_t *iter_buf = (mp_obj_iter_buf_t*)sp;
                    sp += MP_OBJ_ITER_BUF_NSLOTS - 1;
                    obj = mp_getiter(obj, iter_buf);
                    if (obj != MP_OBJ_FROM_PTR(iter_buf)) {
                        
                        *(sp - MP_OBJ_ITER_BUF_NSLOTS + 1) = MP_OBJ_NULL;
                        *(sp - MP_OBJ_ITER_BUF_NSLOTS + 2) = obj;
                    }
                    DISPATCH();
                }

                ENTRY(MP_BC_FOR_ITER): {
                    FRAME_UPDATE();
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_ULABEL; 
                    code_state->sp = sp;
                    mp_obj_t obj;
                    if (*(sp - MP_OBJ_ITER_BUF_NSLOTS + 1) == MP_OBJ_NULL) {
                        obj = *(sp - MP_OBJ_ITER_BUF_NSLOTS + 2);
                    } else {
                        obj = MP_OBJ_FROM_PTR(&sp[-MP_OBJ_ITER_BUF_NSLOTS + 1]);
                    }
                    mp_obj_t value = mp_iternext_allow_raise(obj);
                    if (value == MP_OBJ_STOP_ITERATION) {
                        sp -= MP_OBJ_ITER_BUF_NSLOTS; 
                        ip += ulab; 
                    } else {
                        PUSH(value); 
                        #if MICROPY_PY_SYS_SETTRACE
                        
                        if (code_state->frame) {
                            code_state->frame->lineno = 0;
                        }
                        #endif
                    }
                    DISPATCH();
                }

                ENTRY(MP_BC_POP_EXCEPT_JUMP): {
                    assert(exc_sp >= exc_stack);
                    POP_EXC_BLOCK();
                    DECODE_ULABEL;
                    ip += ulab;
                    DISPATCH_WITH_PEND_EXC_CHECK();
                }

                ENTRY(MP_BC_BUILD_TUPLE): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_UINT;
                    sp -= unum - 1;
                    SET_TOP(mp_obj_new_tuple(unum, sp));
                    DISPATCH();
                }

                ENTRY(MP_BC_BUILD_LIST): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_UINT;
                    sp -= unum - 1;
                    SET_TOP(mp_obj_new_list(unum, sp));
                    DISPATCH();
                }

                ENTRY(MP_BC_BUILD_MAP): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_UINT;
                    PUSH(mp_obj_new_dict(unum));
                    DISPATCH();
                }

                ENTRY(MP_BC_STORE_MAP):
                    MARK_EXC_IP_SELECTIVE();
                    sp -= 2;
                    mp_obj_dict_store(sp[0], sp[2], sp[1]);
                    DISPATCH();

                #if MICROPY_PY_BUILTINS_SET
                ENTRY(MP_BC_BUILD_SET): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_UINT;
                    sp -= unum - 1;
                    SET_TOP(mp_obj_new_set(unum, sp));
                    DISPATCH();
                }
                #endif

                #if MICROPY_PY_BUILTINS_SLICE
                ENTRY(MP_BC_BUILD_SLICE): {
                    MARK_EXC_IP_SELECTIVE();
                    mp_obj_t step = mp_const_none;
                    if (*ip++ == 3) {
                        
                        step = POP();
                    }
                    mp_obj_t stop = POP();
                    mp_obj_t start = TOP();
                    SET_TOP(mp_obj_new_slice(start, stop, step));
                    DISPATCH();
                }
                #endif

                ENTRY(MP_BC_STORE_COMP): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_UINT;
                    mp_obj_t obj = sp[-(unum >> 2)];
                    if ((unum & 3) == 0) {
                        mp_obj_list_append(obj, sp[0]);
                        sp--;
                    } else if (!MICROPY_PY_BUILTINS_SET || (unum & 3) == 1) {
                        mp_obj_dict_store(obj, sp[0], sp[-1]);
                        sp -= 2;
                    #if MICROPY_PY_BUILTINS_SET
                    } else {
                        mp_obj_set_store(obj, sp[0]);
                        sp--;
                    #endif
                    }
                    DISPATCH();
                }

                ENTRY(MP_BC_UNPACK_SEQUENCE): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_UINT;
                    mp_unpack_sequence(sp[0], unum, sp);
                    sp += unum - 1;
                    DISPATCH();
                }

                ENTRY(MP_BC_UNPACK_EX): {
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_UINT;
                    mp_unpack_ex(sp[0], unum, sp);
                    sp += (unum & 0xff) + ((unum >> 8) & 0xff);
                    DISPATCH();
                }

                ENTRY(MP_BC_MAKE_FUNCTION): {
                    DECODE_PTR;
                    PUSH(mp_make_function_from_proto_fun(ptr, code_state->fun_bc->context, NULL));
                    DISPATCH();
                }

                ENTRY(MP_BC_MAKE_FUNCTION_DEFARGS): {
                    DECODE_PTR;
                    
                    sp -= 1;
                    SET_TOP(mp_make_function_from_proto_fun(ptr, code_state->fun_bc->context, sp));
                    DISPATCH();
                }

                ENTRY(MP_BC_MAKE_CLOSURE): {
                    DECODE_PTR;
                    size_t n_closed_over = *ip++;
                    
                    sp -= n_closed_over - 1;
                    SET_TOP(mp_make_closure_from_proto_fun(ptr, code_state->fun_bc->context, n_closed_over, sp));
                    DISPATCH();
                }

                ENTRY(MP_BC_MAKE_CLOSURE_DEFARGS): {
                    DECODE_PTR;
                    size_t n_closed_over = *ip++;
                    
                    sp -= 2 + n_closed_over - 1;
                    SET_TOP(mp_make_closure_from_proto_fun(ptr, code_state->fun_bc->context, 0x100 | n_closed_over, sp));
                    DISPATCH();
                }

                ENTRY(MP_BC_CALL_FUNCTION): {
                    FRAME_UPDATE();
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_UINT;
                    
                    
                    sp -= (unum & 0xff) + ((unum >> 7) & 0x1fe);
                    #if MICROPY_STACKLESS
                    if (mp_obj_get_type(*sp) == &mp_type_fun_bc) {
                        code_state->ip = ip;
                        code_state->sp = sp;
                        code_state->exc_sp_idx = MP_CODE_STATE_EXC_SP_IDX_FROM_PTR(exc_stack, exc_sp);
                        mp_code_state_t *new_state = mp_obj_fun_bc_prepare_codestate(*sp, unum & 0xff, (unum >> 8) & 0xff, sp + 1);
                        #if !MICROPY_ENABLE_PYSTACK
                        if (new_state == NULL) {
                            
                            
                            #if MICROPY_STACKLESS_STRICT
                        deep_recursion_error:
                            mp_raise_recursion_depth();
                            #endif
                        } else
                        #endif
                        {
                            new_state->prev = code_state;
                            code_state = new_state;
                            nlr_pop();
                            goto run_code_state;
                        }
                    }
                    #endif
                    SET_TOP(mp_call_function_n_kw(*sp, unum & 0xff, (unum >> 8) & 0xff, sp + 1));
                    DISPATCH();
                }

                ENTRY(MP_BC_CALL_FUNCTION_VAR_KW): {
                    FRAME_UPDATE();
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_UINT;
                    
                    
                    
                    
                    sp -= (unum & 0xff) + ((unum >> 7) & 0x1fe) + 1;
                    #if MICROPY_STACKLESS
                    if (mp_obj_get_type(*sp) == &mp_type_fun_bc) {
                        code_state->ip = ip;
                        code_state->sp = sp;
                        code_state->exc_sp_idx = MP_CODE_STATE_EXC_SP_IDX_FROM_PTR(exc_stack, exc_sp);

                        mp_call_args_t out_args;
                        mp_call_prepare_args_n_kw_var(false, unum, sp, &out_args);

                        mp_code_state_t *new_state = mp_obj_fun_bc_prepare_codestate(out_args.fun,
                            out_args.n_args, out_args.n_kw, out_args.args);
                        #if !MICROPY_ENABLE_PYSTACK
                        
                        
                        mp_nonlocal_free(out_args.args, out_args.n_alloc * sizeof(mp_obj_t));
                        #endif
                        #if !MICROPY_ENABLE_PYSTACK
                        if (new_state == NULL) {
                            
                            
                            #if MICROPY_STACKLESS_STRICT
                            goto deep_recursion_error;
                            #endif
                        } else
                        #endif
                        {
                            new_state->prev = code_state;
                            code_state = new_state;
                            nlr_pop();
                            goto run_code_state;
                        }
                    }
                    #endif
                    SET_TOP(mp_call_method_n_kw_var(false, unum, sp));
                    DISPATCH();
                }

                ENTRY(MP_BC_CALL_METHOD): {
                    FRAME_UPDATE();
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_UINT;
                    
                    
                    sp -= (unum & 0xff) + ((unum >> 7) & 0x1fe) + 1;
                    #if MICROPY_STACKLESS
                    if (mp_obj_get_type(*sp) == &mp_type_fun_bc) {
                        code_state->ip = ip;
                        code_state->sp = sp;
                        code_state->exc_sp_idx = MP_CODE_STATE_EXC_SP_IDX_FROM_PTR(exc_stack, exc_sp);

                        size_t n_args = unum & 0xff;
                        size_t n_kw = (unum >> 8) & 0xff;
                        int adjust = (sp[1] == MP_OBJ_NULL) ? 0 : 1;

                        mp_code_state_t *new_state = mp_obj_fun_bc_prepare_codestate(*sp, n_args + adjust, n_kw, sp + 2 - adjust);
                        #if !MICROPY_ENABLE_PYSTACK
                        if (new_state == NULL) {
                            
                            
                            #if MICROPY_STACKLESS_STRICT
                            goto deep_recursion_error;
                            #endif
                        } else
                        #endif
                        {
                            new_state->prev = code_state;
                            code_state = new_state;
                            nlr_pop();
                            goto run_code_state;
                        }
                    }
                    #endif
                    SET_TOP(mp_call_method_n_kw(unum & 0xff, (unum >> 8) & 0xff, sp));
                    DISPATCH();
                }

                ENTRY(MP_BC_CALL_METHOD_VAR_KW): {
                    FRAME_UPDATE();
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_UINT;
                    
                    
                    
                    
                    sp -= (unum & 0xff) + ((unum >> 7) & 0x1fe) + 2;
                    #if MICROPY_STACKLESS
                    if (mp_obj_get_type(*sp) == &mp_type_fun_bc) {
                        code_state->ip = ip;
                        code_state->sp = sp;
                        code_state->exc_sp_idx = MP_CODE_STATE_EXC_SP_IDX_FROM_PTR(exc_stack, exc_sp);

                        mp_call_args_t out_args;
                        mp_call_prepare_args_n_kw_var(true, unum, sp, &out_args);

                        mp_code_state_t *new_state = mp_obj_fun_bc_prepare_codestate(out_args.fun,
                            out_args.n_args, out_args.n_kw, out_args.args);
                        #if !MICROPY_ENABLE_PYSTACK
                        
                        
                        mp_nonlocal_free(out_args.args, out_args.n_alloc * sizeof(mp_obj_t));
                        #endif
                        #if !MICROPY_ENABLE_PYSTACK
                        if (new_state == NULL) {
                            
                            
                            #if MICROPY_STACKLESS_STRICT
                            goto deep_recursion_error;
                            #endif
                        } else
                        #endif
                        {
                            new_state->prev = code_state;
                            code_state = new_state;
                            nlr_pop();
                            goto run_code_state;
                        }
                    }
                    #endif
                    SET_TOP(mp_call_method_n_kw_var(true, unum, sp));
                    DISPATCH();
                }

                ENTRY(MP_BC_RETURN_VALUE):
                    MARK_EXC_IP_SELECTIVE();
unwind_return:
                    
                    while (exc_sp >= exc_stack) {
                        if (MP_TAGPTR_TAG1(exc_sp->val_sp)) {
                            if (exc_sp->handler >= ip) {
                                
                                
                                
                                
                                
                                
                                
                                
                                
                                
                                mp_obj_t *finally_sp = MP_TAGPTR_PTR(exc_sp->val_sp);
                                finally_sp[1] = sp[0];
                                sp = &finally_sp[1];
                                
                                
                                
                                
                                PUSH(MP_OBJ_NEW_SMALL_INT(-1));
                                ip = exc_sp->handler;
                                goto dispatch_loop;
                            } else {
                                
                                CANCEL_ACTIVE_FINALLY(sp);
                            }
                        }
                        POP_EXC_BLOCK();
                    }
                    nlr_pop();
                    code_state->sp = sp;
                    assert(exc_sp == exc_stack - 1);
                    MICROPY_VM_HOOK_RETURN
                    #if MICROPY_STACKLESS
                    if (code_state->prev != NULL) {
                        mp_obj_t res = *sp;
                        mp_globals_set(code_state->old_globals);
                        mp_code_state_t *new_code_state = code_state->prev;
                        #if MICROPY_ENABLE_PYSTACK
                        
                        
                        
                        
                        mp_nonlocal_free(code_state, sizeof(mp_code_state_t));
                        #endif
                        code_state = new_code_state;
                        *code_state->sp = res;
                        goto run_code_state_from_return;
                    }
                    #endif
                    FRAME_LEAVE();
                    return MP_VM_RETURN_NORMAL;

                ENTRY(MP_BC_RAISE_LAST): {
                    MARK_EXC_IP_SELECTIVE();
                    
                    mp_obj_t obj = MP_OBJ_NULL;
                    for (mp_exc_stack_t *e = exc_sp; e >= exc_stack; --e) {
                        if (e->prev_exc != NULL) {
                            obj = MP_OBJ_FROM_PTR(e->prev_exc);
                            break;
                        }
                    }
                    if (obj == MP_OBJ_NULL) {
                        obj = mp_obj_new_exception_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("no active exception to reraise"));
                    }
                    RAISE(obj);
                }

                ENTRY(MP_BC_RAISE_OBJ): {
                    MARK_EXC_IP_SELECTIVE();
                    mp_obj_t obj = mp_make_raise_obj(TOP());
                    RAISE(obj);
                }

                ENTRY(MP_BC_RAISE_FROM): {
                    MARK_EXC_IP_SELECTIVE();
                    mp_obj_t from_value = POP();
                    if (from_value != mp_const_none) {
                        mp_warning(NULL, "exception chaining not supported");
                    }
                    mp_obj_t obj = mp_make_raise_obj(TOP());
                    RAISE(obj);
                }

                ENTRY(MP_BC_YIELD_VALUE):
yield:
                    nlr_pop();
                    code_state->ip = ip;
                    code_state->sp = sp;
                    code_state->exc_sp_idx = MP_CODE_STATE_EXC_SP_IDX_FROM_PTR(exc_stack, exc_sp);
                    FRAME_LEAVE();
                    return MP_VM_RETURN_YIELD;

                ENTRY(MP_BC_YIELD_FROM): {
                    MARK_EXC_IP_SELECTIVE();
                    mp_vm_return_kind_t ret_kind;
                    mp_obj_t send_value = POP();
                    mp_obj_t t_exc = MP_OBJ_NULL;
                    mp_obj_t ret_value;
                    code_state->sp = sp; 
                    if (inject_exc != MP_OBJ_NULL) {
                        t_exc = inject_exc;
                        inject_exc = MP_OBJ_NULL;
                        ret_kind = mp_resume(TOP(), MP_OBJ_NULL, t_exc, &ret_value);
                    } else {
                        ret_kind = mp_resume(TOP(), send_value, MP_OBJ_NULL, &ret_value);
                    }

                    if (ret_kind == MP_VM_RETURN_YIELD) {
                        ip--;
                        PUSH(ret_value);
                        goto yield;
                    } else if (ret_kind == MP_VM_RETURN_NORMAL) {
                        
                        
                        SET_TOP(ret_value);
                        
                        
                        if (t_exc != MP_OBJ_NULL && mp_obj_exception_match(t_exc, MP_OBJ_FROM_PTR(&mp_type_GeneratorExit))) {
                            mp_obj_t raise_t = mp_make_raise_obj(t_exc);
                            RAISE(raise_t);
                        }
                        DISPATCH();
                    } else {
                        assert(ret_kind == MP_VM_RETURN_EXCEPTION);
                        assert(!mp_obj_exception_match(ret_value, MP_OBJ_FROM_PTR(&mp_type_StopIteration)));
                        
                        sp--;
                        RAISE(ret_value);
                    }
                }

                ENTRY(MP_BC_IMPORT_NAME): {
                    FRAME_UPDATE();
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_QSTR;
                    mp_obj_t obj = POP();
                    SET_TOP(mp_import_name(qst, obj, TOP()));
                    DISPATCH();
                }

                ENTRY(MP_BC_IMPORT_FROM): {
                    FRAME_UPDATE();
                    MARK_EXC_IP_SELECTIVE();
                    DECODE_QSTR;
                    mp_obj_t obj = mp_import_from(TOP(), qst);
                    PUSH(obj);
                    DISPATCH();
                }

                ENTRY(MP_BC_IMPORT_STAR):
                    MARK_EXC_IP_SELECTIVE();
                    mp_import_all(POP());
                    DISPATCH();

                #if MICROPY_OPT_COMPUTED_GOTO
                ENTRY(MP_BC_LOAD_CONST_SMALL_INT_MULTI):
                    PUSH(MP_OBJ_NEW_SMALL_INT((mp_int_t)ip[-1] - MP_BC_LOAD_CONST_SMALL_INT_MULTI - MP_BC_LOAD_CONST_SMALL_INT_MULTI_EXCESS));
                    DISPATCH();

                ENTRY(MP_BC_LOAD_FAST_MULTI):
                    obj_shared = fastn[MP_BC_LOAD_FAST_MULTI - (mp_int_t)ip[-1]];
                    goto load_check;

                ENTRY(MP_BC_STORE_FAST_MULTI):
                    fastn[MP_BC_STORE_FAST_MULTI - (mp_int_t)ip[-1]] = POP();
                    DISPATCH();

                ENTRY(MP_BC_UNARY_OP_MULTI):
                    MARK_EXC_IP_SELECTIVE();
                    SET_TOP(mp_unary_op(ip[-1] - MP_BC_UNARY_OP_MULTI, TOP()));
                    DISPATCH();

                ENTRY(MP_BC_BINARY_OP_MULTI): {
                    MARK_EXC_IP_SELECTIVE();
                    mp_obj_t rhs = POP();
                    mp_obj_t lhs = TOP();
                    SET_TOP(mp_binary_op(ip[-1] - MP_BC_BINARY_OP_MULTI, lhs, rhs));
                    DISPATCH();
                }

                ENTRY_DEFAULT:
                    MARK_EXC_IP_SELECTIVE();
                #else
                ENTRY_DEFAULT:
                    if (ip[-1] < MP_BC_LOAD_CONST_SMALL_INT_MULTI + MP_BC_LOAD_CONST_SMALL_INT_MULTI_NUM) {
                        PUSH(MP_OBJ_NEW_SMALL_INT((mp_int_t)ip[-1] - MP_BC_LOAD_CONST_SMALL_INT_MULTI - MP_BC_LOAD_CONST_SMALL_INT_MULTI_EXCESS));
                        DISPATCH();
                    } else if (ip[-1] < MP_BC_LOAD_FAST_MULTI + MP_BC_LOAD_FAST_MULTI_NUM) {
                        obj_shared = fastn[MP_BC_LOAD_FAST_MULTI - (mp_int_t)ip[-1]];
                        goto load_check;
                    } else if (ip[-1] < MP_BC_STORE_FAST_MULTI + MP_BC_STORE_FAST_MULTI_NUM) {
                        fastn[MP_BC_STORE_FAST_MULTI - (mp_int_t)ip[-1]] = POP();
                        DISPATCH();
                    } else if (ip[-1] < MP_BC_UNARY_OP_MULTI + MP_BC_UNARY_OP_MULTI_NUM) {
                        SET_TOP(mp_unary_op(ip[-1] - MP_BC_UNARY_OP_MULTI, TOP()));
                        DISPATCH();
                    } else if (ip[-1] < MP_BC_BINARY_OP_MULTI + MP_BC_BINARY_OP_MULTI_NUM) {
                        mp_obj_t rhs = POP();
                        mp_obj_t lhs = TOP();
                        SET_TOP(mp_binary_op(ip[-1] - MP_BC_BINARY_OP_MULTI, lhs, rhs));
                        DISPATCH();
                    } else
                #endif 
                {
                    mp_obj_t obj = mp_obj_new_exception_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT("opcode"));
                    nlr_pop();
                    code_state->state[0] = obj;
                    FRAME_LEAVE();
                    return MP_VM_RETURN_EXCEPTION;
                }

                #if !MICROPY_OPT_COMPUTED_GOTO
                } 
                #endif

pending_exception_check:
                
                
                
                
                MICROPY_VM_HOOK_LOOP

                
                
                
                
                if (
                #if MICROPY_ENABLE_SCHEDULER
                #if MICROPY_PY_THREAD
                    
                    MP_STATE_VM(sched_state) == MP_SCHED_PENDING || MP_STATE_THREAD(mp_pending_exception) != MP_OBJ_NULL
                #else
                    
                    MP_STATE_VM(sched_state) == MP_SCHED_PENDING
                #endif
                #else
                    
                    MP_STATE_THREAD(mp_pending_exception) != MP_OBJ_NULL
                #endif
                #if MICROPY_ENABLE_VM_ABORT
                    
                    || MP_STATE_VM(vm_abort)
                #endif
                ) {
                    MARK_EXC_IP_SELECTIVE();
                    mp_handle_pending(true);
                }

                #if MICROPY_PY_THREAD_GIL
                #if MICROPY_PY_THREAD_GIL_VM_DIVISOR
                
                if (--gil_divisor == 0)
                #endif
                {
                    #if MICROPY_PY_THREAD_GIL_VM_DIVISOR
                    gil_divisor = MICROPY_PY_THREAD_GIL_VM_DIVISOR;
                    #endif
                    #if MICROPY_ENABLE_SCHEDULER
                    
                    if (MP_STATE_VM(sched_state) == MP_SCHED_IDLE)
                    #endif
                    {
                    MP_THREAD_GIL_EXIT();
                    MP_THREAD_GIL_ENTER();
                    }
                }
                #endif

            } 

        } else {
exception_handler:
            

            #if MICROPY_PY_SYS_EXC_INFO
            MP_STATE_VM(cur_exception) = nlr.ret_val;
            #endif

            #if SELECTIVE_EXC_IP
            
            code_state->ip -= 1;
            #endif

            if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(((mp_obj_base_t*)nlr.ret_val)->type), MP_OBJ_FROM_PTR(&mp_type_StopIteration))) {
                
                if (*code_state->ip == MP_BC_FOR_ITER) {
                    const byte *ip = code_state->ip + 1;
                    DECODE_ULABEL; 
                    code_state->ip = ip + ulab; 
                    code_state->sp -= MP_OBJ_ITER_BUF_NSLOTS; 
                    goto outer_dispatch_loop; 
                } else if (*code_state->ip == MP_BC_YIELD_FROM) {
                    
                    
                    
                    *code_state->sp = mp_obj_exception_get_value(MP_OBJ_FROM_PTR(nlr.ret_val));
                    code_state->ip++; 
                    goto outer_dispatch_loop; 
                }
            }

            #if MICROPY_PY_SYS_SETTRACE
            
            if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(((mp_obj_base_t*)nlr.ret_val)->type), MP_OBJ_FROM_PTR(&mp_type_Exception))) {
                TRACE_TICK(code_state->ip, code_state->sp, true  );
            }
            #endif

#if MICROPY_STACKLESS
unwind_loop:
#endif
             
             
            
            
            if (nlr.ret_val != &mp_const_GeneratorExit_obj
                && *code_state->ip != MP_BC_END_FINALLY
                && *code_state->ip != MP_BC_RAISE_LAST) {
                const byte *ip = code_state->fun_bc->bytecode;
                MP_BC_PRELUDE_SIG_DECODE(ip);
                MP_BC_PRELUDE_SIZE_DECODE(ip);
                const byte *line_info_top = ip + n_info;
                const byte *bytecode_start = ip + n_info + n_cell;
                size_t bc = code_state->ip - bytecode_start;
                qstr block_name = mp_decode_uint_value(ip);
                for (size_t i = 0; i < 1 + n_pos_args + n_kwonly_args; ++i) {
                    ip = mp_decode_uint_skip(ip);
                }
                #if MICROPY_EMIT_BYTECODE_USES_QSTR_TABLE
                block_name = code_state->fun_bc->context->constants.qstr_table[block_name];
                qstr source_file = code_state->fun_bc->context->constants.qstr_table[0];
                #else
                qstr source_file = code_state->fun_bc->context->constants.source_file;
                #endif
                size_t source_line = mp_bytecode_get_source_line(ip, line_info_top, bc);
                mp_obj_exception_add_traceback(MP_OBJ_FROM_PTR(nlr.ret_val), source_file, source_line, block_name);
            }

            while (exc_sp >= exc_stack && exc_sp->handler <= code_state->ip) {

                

                assert(exc_sp >= exc_stack);

                
                

                
                POP_EXC_BLOCK();
            }

            if (exc_sp >= exc_stack) {
                
                code_state->ip = exc_sp->handler;
                mp_obj_t *sp = MP_TAGPTR_PTR(exc_sp->val_sp);
                
                exc_sp->prev_exc = nlr.ret_val;
                
                PUSH(MP_OBJ_FROM_PTR(nlr.ret_val));
                code_state->sp = sp;

            #if MICROPY_STACKLESS
            } else if (code_state->prev != NULL) {
                mp_globals_set(code_state->old_globals);
                mp_code_state_t *new_code_state = code_state->prev;
                #if MICROPY_ENABLE_PYSTACK
                
                
                
                
                mp_nonlocal_free(code_state, sizeof(mp_code_state_t));
                #endif
                code_state = new_code_state;
                size_t n_state = code_state->n_state;
                fastn = &code_state->state[n_state - 1];
                exc_stack = (mp_exc_stack_t*)(code_state->state + n_state);
                
                exc_sp = MP_CODE_STATE_EXC_SP_IDX_TO_PTR(exc_stack, code_state->exc_sp_idx); 
                goto unwind_loop;

            #endif
            } else {
                
                
                code_state->state[0] = MP_OBJ_FROM_PTR(nlr.ret_val); 
                FRAME_LEAVE();
                return MP_VM_RETURN_EXCEPTION;
            }
        }
    }
}
