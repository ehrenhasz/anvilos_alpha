 
 

#ifndef HVA_MEM_H
#define HVA_MEM_H

 
struct hva_buffer {
	const char		*name;
	dma_addr_t		paddr;
	void			*vaddr;
	u32			size;
};

int hva_mem_alloc(struct hva_ctx *ctx,
		  __u32 size,
		  const char *name,
		  struct hva_buffer **buf);

void hva_mem_free(struct hva_ctx *ctx,
		  struct hva_buffer *buf);

#endif  
