
#ifndef MICROPY_INCLUDED_MODOPENAMP_REMOTEPROC_H
#define MICROPY_INCLUDED_MODOPENAMP_REMOTEPROC_H

#include "openamp/remoteproc.h"
#include "openamp/remoteproc_loader.h"

void *mp_openamp_remoteproc_store_alloc(void);
struct remoteproc *mp_openamp_remoteproc_init(struct remoteproc *rproc,
    const struct remoteproc_ops *ops, void *arg);
void *mp_openamp_remoteproc_mmap(struct remoteproc *rproc, metal_phys_addr_t *pa,
    metal_phys_addr_t *da, size_t size, unsigned int attribute,
    struct metal_io_region **io);
int mp_openamp_remoteproc_start(struct remoteproc *rproc);
int mp_openamp_remoteproc_stop(struct remoteproc *rproc);
int mp_openamp_remoteproc_config(struct remoteproc *rproc, void *data);
void mp_openamp_remoteproc_remove(struct remoteproc *rproc);
int mp_openamp_remoteproc_shutdown(struct remoteproc *rproc);

#endif 
