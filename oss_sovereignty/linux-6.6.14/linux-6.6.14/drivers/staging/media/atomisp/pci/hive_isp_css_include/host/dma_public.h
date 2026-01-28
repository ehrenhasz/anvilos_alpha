#ifndef __DMA_PUBLIC_H_INCLUDED__
#define __DMA_PUBLIC_H_INCLUDED__
#include "system_local.h"
typedef struct dma_state_s		dma_state_t;
void dma_get_state(
    const dma_ID_t		ID,
    dma_state_t			*state);
STORAGE_CLASS_DMA_H void dma_reg_store(
    const dma_ID_t		ID,
    const unsigned int	reg,
    const hrt_data		value);
STORAGE_CLASS_DMA_H hrt_data dma_reg_load(
    const dma_ID_t		ID,
    const unsigned int	reg);
void
dma_set_max_burst_size(
    dma_ID_t		ID,
    dma_connection		conn,
    uint32_t		max_burst_size);
#endif  
