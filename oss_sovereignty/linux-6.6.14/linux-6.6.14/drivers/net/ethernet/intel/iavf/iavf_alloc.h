#ifndef _IAVF_ALLOC_H_
#define _IAVF_ALLOC_H_
struct iavf_hw;
enum iavf_memory_type {
	iavf_mem_arq_buf = 0,		 
	iavf_mem_asq_buf = 1,
	iavf_mem_atq_buf = 2,		 
	iavf_mem_arq_ring = 3,		 
	iavf_mem_atq_ring = 4,		 
	iavf_mem_pd = 5,		 
	iavf_mem_bp = 6,		 
	iavf_mem_bp_jumbo = 7,		 
	iavf_mem_reserved
};
enum iavf_status iavf_allocate_dma_mem(struct iavf_hw *hw,
				       struct iavf_dma_mem *mem,
				       enum iavf_memory_type type,
				       u64 size, u32 alignment);
enum iavf_status iavf_free_dma_mem(struct iavf_hw *hw,
				   struct iavf_dma_mem *mem);
enum iavf_status iavf_allocate_virt_mem(struct iavf_hw *hw,
					struct iavf_virt_mem *mem, u32 size);
void iavf_free_virt_mem(struct iavf_hw *hw, struct iavf_virt_mem *mem);
#endif  
