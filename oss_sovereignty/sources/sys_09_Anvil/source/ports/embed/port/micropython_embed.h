
#ifndef MICROPY_INCLUDED_MICROPYTHON_EMBED_H
#define MICROPY_INCLUDED_MICROPYTHON_EMBED_H

#include <stddef.h>
#include <stdint.h>

void mp_embed_init(void *gc_heap, size_t gc_heap_size, void *stack_top);
void mp_embed_deinit(void);


void mp_embed_exec_str(const char *src);


void mp_embed_exec_mpy(const uint8_t *mpy, size_t len);

#endif 
