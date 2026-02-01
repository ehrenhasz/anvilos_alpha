 

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "py/mpstate.h"
#include "py/gc.h"

#if MICROPY_EMIT_NATIVE || (MICROPY_PY_FFI && MICROPY_FORCE_PLAT_ALLOC_EXEC)

#if defined(__OpenBSD__) || defined(__MACH__)
#define MAP_ANONYMOUS MAP_ANON
#endif





typedef struct _mmap_region_t {
    void *ptr;
    size_t len;
    struct _mmap_region_t *next;
} mmap_region_t;

void mp_unix_alloc_exec(size_t min_size, void **ptr, size_t *size) {
    
    *size = (min_size + 0xfff) & (~0xfff);
    *ptr = mmap(NULL, *size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (*ptr == MAP_FAILED) {
        *ptr = NULL;
    }

    
    mmap_region_t *rg = m_new_obj(mmap_region_t);
    rg->ptr = *ptr;
    rg->len = min_size;
    rg->next = MP_STATE_VM(mmap_region_head);
    MP_STATE_VM(mmap_region_head) = rg;
}

void mp_unix_free_exec(void *ptr, size_t size) {
    munmap(ptr, size);

    
    for (mmap_region_t **rg = (mmap_region_t **)&MP_STATE_VM(mmap_region_head); *rg != NULL; *rg = (*rg)->next) {
        if ((*rg)->ptr == ptr) {
            mmap_region_t *next = (*rg)->next;
            m_del_obj(mmap_region_t, *rg);
            *rg = next;
            return;
        }
    }
}

void mp_unix_mark_exec(void) {
    for (mmap_region_t *rg = MP_STATE_VM(mmap_region_head); rg != NULL; rg = rg->next) {
        gc_collect_root(rg->ptr, rg->len / sizeof(mp_uint_t));
    }
}

#if MICROPY_FORCE_PLAT_ALLOC_EXEC



void *ffi_closure_alloc(size_t size, void **code);
void ffi_closure_free(void *ptr);

void *ffi_closure_alloc(size_t size, void **code) {
    size_t dummy;
    mp_unix_alloc_exec(size, code, &dummy);
    return *code;
}

void ffi_closure_free(void *ptr) {
    (void)ptr;
    
}
#endif

MP_REGISTER_ROOT_POINTER(void *mmap_region_head);

#endif 
