 
#ifndef MICROPY_INCLUDED_METAL_MICROPYTHON_H
#define MICROPY_INCLUDED_METAL_MICROPYTHON_H

#include <stdlib.h>


#include "mpmetalport.h"


#define METAL_VER_MAJOR     1


#define METAL_VER_MINOR     5


#define METAL_VER_PATCH     0


#define METAL_VER           "1.5.0"

#if METAL_HAVE_STDATOMIC_H
#define HAVE_STDATOMIC_H
#endif

#if METAL_HAVE_FUTEX_H
#define HAVE_FUTEX_H
#endif

#ifndef METAL_MAX_DEVICE_REGIONS
#define METAL_MAX_DEVICE_REGIONS 1
#endif

static inline void *__metal_allocate_memory(unsigned int size) {
    return m_tracked_calloc(1, size);
}

static inline void __metal_free_memory(void *ptr) {
    m_tracked_free(ptr);
}


int __metal_sleep_usec(unsigned int usec);
void sys_irq_enable(unsigned int vector);
void sys_irq_disable(unsigned int vector);
void sys_irq_restore_enable(unsigned int flags);
unsigned int sys_irq_save_disable(void);
#endif 
