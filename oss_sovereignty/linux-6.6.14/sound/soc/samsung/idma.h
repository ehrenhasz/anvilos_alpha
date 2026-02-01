 
 

#ifndef __SND_SOC_SAMSUNG_IDMA_H_
#define __SND_SOC_SAMSUNG_IDMA_H_

extern void idma_reg_addr_init(void __iomem *regs, dma_addr_t addr);

 
#define LPAM_DMA_STOP	0
#define LPAM_DMA_START	1

#define MAX_IDMA_PERIOD (128 * 1024)
#define MAX_IDMA_BUFFER (160 * 1024)

#endif  
