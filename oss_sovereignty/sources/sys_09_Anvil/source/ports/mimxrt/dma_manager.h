
#ifndef MICROPY_INCLUDED_MIMXRT_DMACHANNEL_H
#define MICROPY_INCLUDED_MIMXRT_DMACHANNEL_H

#include "py/runtime.h"

int allocate_dma_channel(void);
void free_dma_channel(int n);
void dma_init(void);

#endif 
