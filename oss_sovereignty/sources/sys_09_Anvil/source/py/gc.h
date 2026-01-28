
#ifndef MICROPY_INCLUDED_PY_GC_H
#define MICROPY_INCLUDED_PY_GC_H

#include <stdbool.h>
#include <stddef.h>
#include "py/mpprint.h"

void gc_init(void *start, void *end);

#if MICROPY_GC_SPLIT_HEAP

void gc_add(void *start, void *end);

#if MICROPY_GC_SPLIT_HEAP_AUTO


size_t gc_get_max_new_split(void);
#endif 
#endif 



void gc_lock(void);
void gc_unlock(void);
bool gc_is_locked(void);


void gc_collect(void);
void gc_collect_start(void);
void gc_collect_root(void **ptrs, size_t len);
void gc_collect_end(void);


void gc_sweep_all(void);

enum {
    GC_ALLOC_FLAG_HAS_FINALISER = 1,
};

void *gc_alloc(size_t n_bytes, unsigned int alloc_flags);
void gc_free(void *ptr); 
size_t gc_nbytes(const void *ptr);
void *gc_realloc(void *ptr, size_t n_bytes, bool allow_move);

typedef struct _gc_info_t {
    size_t total;
    size_t used;
    size_t free;
    size_t max_free;
    size_t num_1block;
    size_t num_2block;
    size_t max_block;
    #if MICROPY_GC_SPLIT_HEAP_AUTO
    size_t max_new_split;
    #endif
} gc_info_t;

void gc_info(gc_info_t *info);
void gc_dump_info(const mp_print_t *print);
void gc_dump_alloc_table(const mp_print_t *print);

#endif 
