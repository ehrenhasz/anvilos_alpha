#ifndef _XTENSA_DMA_H
#define _XTENSA_DMA_H
#include <asm/io.h>		 
#define MAX_DMA_CHANNELS	8
#ifndef MAX_DMA_ADDRESS
#define MAX_DMA_ADDRESS		(PAGE_OFFSET + XCHAL_KIO_SIZE - 1)
#endif
extern int request_dma(unsigned int dmanr, const char * device_id);
extern void free_dma(unsigned int dmanr);
#endif
