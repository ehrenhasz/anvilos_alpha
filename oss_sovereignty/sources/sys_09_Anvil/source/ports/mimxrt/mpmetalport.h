
#ifndef MICROPY_INCLUDED_MIMXRT_MPMETALPORT_H
#define MICROPY_INCLUDED_MIMXRT_MPMETALPORT_H

#include <stdlib.h>
#include "py/mphal.h"
#include "py/runtime.h"

#define METAL_HAVE_STDATOMIC_H      0
#define METAL_HAVE_FUTEX_H          0

#define METAL_MAX_DEVICE_REGIONS    2


#define METAL_LOG_HANDLER_ENABLE    0

#define metal_cpu_yield()


#define METAL_SHM_NAME              "OPENAMP_SHM"


#define METAL_RSC_ADDR              ((void *)_openamp_shm_region_start)
#define METAL_RSC_SIZE              (1024)

#define METAL_SHM_ADDR              ((metal_phys_addr_t)(_openamp_shm_region_start + METAL_RSC_SIZE))
#define METAL_SHM_SIZE              ((size_t)(_openamp_shm_region_end - _openamp_shm_region_start - METAL_RSC_SIZE))

#define METAL_MPU_REGION_BASE       ((uint32_t)_openamp_shm_region_start)
#define METAL_MPU_REGION_SIZE       (ARM_MPU_REGION_SIZE_64KB)

extern const char _openamp_shm_region_start[];
extern const char _openamp_shm_region_end[];

int metal_rproc_notify(void *priv, uint32_t id);
extern void openamp_remoteproc_notified(mp_sched_node_t *node);

static inline int __metal_sleep_usec(unsigned int usec) {
    mp_hal_delay_us(usec);
    return 0;
}

static inline void metal_generic_default_poll(void) {
    MICROPY_EVENT_POLL_HOOK
}

#endif 
