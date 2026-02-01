 

#include "py/mpstate.h"

#if !MICROPY_NLR_SETJMP

#if MICROPY_NLR_X86 && MICROPY_NLR_OS_WINDOWS

unsigned int nlr_push_tail(nlr_buf_t *nlr) asm ("nlr_push_tail");
#else

__attribute__((used)) unsigned int nlr_push_tail(nlr_buf_t *nlr);
#endif
#endif

unsigned int nlr_push_tail(nlr_buf_t *nlr) {
    nlr_buf_t **top = &MP_STATE_THREAD(nlr_top);
    nlr->prev = *top;
    MP_NLR_SAVE_PYSTACK(nlr);
    *top = nlr;
    return 0; 
}

void nlr_pop(void) {
    nlr_buf_t **top = &MP_STATE_THREAD(nlr_top);
    *top = (*top)->prev;
}

void nlr_push_jump_callback(nlr_jump_callback_node_t *node, nlr_jump_callback_fun_t fun) {
    nlr_jump_callback_node_t **top = &MP_STATE_THREAD(nlr_jump_callback_top);
    node->prev = *top;
    node->fun = fun;
    *top = node;
}

void nlr_pop_jump_callback(bool run_callback) {
    nlr_jump_callback_node_t **top = &MP_STATE_THREAD(nlr_jump_callback_top);
    nlr_jump_callback_node_t *cur = *top;
    *top = (*top)->prev;
    if (run_callback) {
        cur->fun(cur);
    }
}








void nlr_call_jump_callbacks(nlr_buf_t *nlr) {
    nlr_jump_callback_node_t **top = &MP_STATE_THREAD(nlr_jump_callback_top);
    while (*top != NULL && (void *)*top < (void *)nlr) {
        nlr_pop_jump_callback(true);
    }
}

#if MICROPY_ENABLE_VM_ABORT
NORETURN void nlr_jump_abort(void) {
    MP_STATE_THREAD(nlr_top) = MP_STATE_VM(nlr_abort);
    nlr_jump(NULL);
}
#endif
