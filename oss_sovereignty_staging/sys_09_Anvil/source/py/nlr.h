 
#ifndef MICROPY_INCLUDED_PY_NLR_H
#define MICROPY_INCLUDED_PY_NLR_H




#include <limits.h>
#include <assert.h>
#include <stdbool.h>

#include "py/mpconfig.h"

#define MICROPY_NLR_NUM_REGS_X86            (6)
#define MICROPY_NLR_NUM_REGS_X64            (8)
#define MICROPY_NLR_NUM_REGS_X64_WIN        (10)
#define MICROPY_NLR_NUM_REGS_ARM_THUMB      (10)
#define MICROPY_NLR_NUM_REGS_ARM_THUMB_FP   (10 + 6)
#define MICROPY_NLR_NUM_REGS_AARCH64        (13)
#define MICROPY_NLR_NUM_REGS_MIPS           (13)
#define MICROPY_NLR_NUM_REGS_XTENSA         (10)
#define MICROPY_NLR_NUM_REGS_XTENSAWIN      (17)




#if !MICROPY_NLR_SETJMP

#if defined(_WIN32) || defined(__CYGWIN__)
#define MICROPY_NLR_OS_WINDOWS 1
#else
#define MICROPY_NLR_OS_WINDOWS 0
#endif
#if defined(__i386__)
    #define MICROPY_NLR_X86 (1)
    #define MICROPY_NLR_NUM_REGS (MICROPY_NLR_NUM_REGS_X86)
#elif defined(__x86_64__)
    #define MICROPY_NLR_X64 (1)
    #if MICROPY_NLR_OS_WINDOWS
        #define MICROPY_NLR_NUM_REGS (MICROPY_NLR_NUM_REGS_X64_WIN)
    #else
        #define MICROPY_NLR_NUM_REGS (MICROPY_NLR_NUM_REGS_X64)
    #endif
#elif defined(__thumb2__) || defined(__thumb__) || defined(__arm__)
    #define MICROPY_NLR_THUMB (1)
    #if defined(__SOFTFP__)
        #define MICROPY_NLR_NUM_REGS (MICROPY_NLR_NUM_REGS_ARM_THUMB)
    #else
        
        
        
        #define MICROPY_NLR_NUM_REGS (MICROPY_NLR_NUM_REGS_ARM_THUMB_FP)
    #endif
#elif defined(__aarch64__)
    #define MICROPY_NLR_AARCH64 (1)
    #define MICROPY_NLR_NUM_REGS (MICROPY_NLR_NUM_REGS_AARCH64)
#elif defined(__xtensa__)
    #define MICROPY_NLR_XTENSA (1)
    #define MICROPY_NLR_NUM_REGS (MICROPY_NLR_NUM_REGS_XTENSA)
#elif defined(__powerpc__)
    #define MICROPY_NLR_POWERPC (1)
    
    #define MICROPY_NLR_NUM_REGS (128)
#elif defined(__mips__)
    #define MICROPY_NLR_MIPS (1)
    #define MICROPY_NLR_NUM_REGS (MICROPY_NLR_NUM_REGS_MIPS)
#else
    #define MICROPY_NLR_SETJMP (1)
    
#endif
#endif



#if MICROPY_NLR_SETJMP
#include <setjmp.h>
#endif

typedef struct _nlr_buf_t nlr_buf_t;
struct _nlr_buf_t {
    

    
    
    nlr_buf_t *prev;

    
    
    
    void *ret_val;

    #if MICROPY_NLR_SETJMP
    jmp_buf jmpbuf;
    #else
    void *regs[MICROPY_NLR_NUM_REGS];
    #endif

    #if MICROPY_ENABLE_PYSTACK
    void *pystack;
    #endif
};

typedef void (*nlr_jump_callback_fun_t)(void *ctx);

typedef struct _nlr_jump_callback_node_t nlr_jump_callback_node_t;

struct _nlr_jump_callback_node_t {
    nlr_jump_callback_node_t *prev;
    nlr_jump_callback_fun_t fun;
};


#if MICROPY_ENABLE_PYSTACK
#define MP_NLR_SAVE_PYSTACK(nlr_buf) (nlr_buf)->pystack = MP_STATE_THREAD(pystack_cur)
#define MP_NLR_RESTORE_PYSTACK(nlr_buf) MP_STATE_THREAD(pystack_cur) = (nlr_buf)->pystack
#else
#define MP_NLR_SAVE_PYSTACK(nlr_buf) (void)nlr_buf
#define MP_NLR_RESTORE_PYSTACK(nlr_buf) (void)nlr_buf
#endif


#define MP_NLR_JUMP_HEAD(val, top) \
    nlr_buf_t **_top_ptr = &MP_STATE_THREAD(nlr_top); \
    nlr_buf_t *top = *_top_ptr; \
    if (top == NULL) { \
        nlr_jump_fail(val); \
    } \
    top->ret_val = val; \
    nlr_call_jump_callbacks(top); \
    MP_NLR_RESTORE_PYSTACK(top); \
    *_top_ptr = top->prev; \

#if MICROPY_NLR_SETJMP



#define nlr_push(buf) (nlr_push_tail(buf), setjmp((buf)->jmpbuf))
#else
unsigned int nlr_push(nlr_buf_t *);
#endif

unsigned int nlr_push_tail(nlr_buf_t *top);
void nlr_pop(void);
NORETURN void nlr_jump(void *val);

#if MICROPY_ENABLE_VM_ABORT
#define nlr_set_abort(buf) MP_STATE_VM(nlr_abort) = buf
#define nlr_get_abort() MP_STATE_VM(nlr_abort)
NORETURN void nlr_jump_abort(void);
#endif




NORETURN void nlr_jump_fail(void *val);


#ifndef MICROPY_DEBUG_NLR
#define nlr_raise(val) nlr_jump(MP_OBJ_TO_PTR(val))
#else

#define nlr_raise(val) \
    do { \
        void *_val = MP_OBJ_TO_PTR(val); \
        assert(_val != NULL); \
        assert(mp_obj_is_exception_instance(val)); \
        nlr_jump(_val); \
    } while (0)

#if !MICROPY_NLR_SETJMP
#define nlr_push(val) \
    assert(MP_STATE_THREAD(nlr_top) != val), nlr_push(val)
#endif

#endif




void nlr_push_jump_callback(nlr_jump_callback_node_t *node, nlr_jump_callback_fun_t fun);



void nlr_pop_jump_callback(bool run_callback);


void nlr_call_jump_callbacks(nlr_buf_t *nlr);

#endif 
