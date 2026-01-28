#ifndef __MACH_SRAM_H
#define __MACH_SRAM_H
#define SRAM_GRANULARITY	512
extern void *sram_alloc(size_t len, dma_addr_t *dma);
extern void sram_free(void *addr, size_t len);
extern struct gen_pool *sram_get_gen_pool(void);
#endif  
