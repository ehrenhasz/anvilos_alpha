 
 

#ifndef _CC_LLI_DEFS_H_
#define _CC_LLI_DEFS_H_

#include <linux/types.h>

 
#define DLLI_SIZE_BIT_SIZE	0x18

#define CC_MAX_MLLI_ENTRY_SIZE 0xFFFF

#define LLI_MAX_NUM_OF_DATA_ENTRIES 128
#define LLI_MAX_NUM_OF_ASSOC_DATA_ENTRIES 8
#define MLLI_TABLE_MIN_ALIGNMENT 4  
#define MAX_NUM_OF_BUFFERS_IN_MLLI 4
#define MAX_NUM_OF_TOTAL_MLLI_ENTRIES \
		(2 * LLI_MAX_NUM_OF_DATA_ENTRIES + \
		 LLI_MAX_NUM_OF_ASSOC_DATA_ENTRIES)

 
#define LLI_ENTRY_WORD_SIZE 2
#define LLI_ENTRY_BYTE_SIZE (LLI_ENTRY_WORD_SIZE * sizeof(u32))

 
#define LLI_WORD0_OFFSET 0
#define LLI_LADDR_BIT_OFFSET 0
#define LLI_LADDR_BIT_SIZE 32
 
#define LLI_WORD1_OFFSET 1
#define LLI_SIZE_BIT_OFFSET 0
#define LLI_SIZE_BIT_SIZE 16
#define LLI_HADDR_BIT_OFFSET 16
#define LLI_HADDR_BIT_SIZE 16

#define LLI_SIZE_MASK GENMASK((LLI_SIZE_BIT_SIZE - 1), LLI_SIZE_BIT_OFFSET)
#define LLI_HADDR_MASK GENMASK( \
			       (LLI_HADDR_BIT_OFFSET + LLI_HADDR_BIT_SIZE - 1),\
				LLI_HADDR_BIT_OFFSET)

static inline void cc_lli_set_addr(u32 *lli_p, dma_addr_t addr)
{
	lli_p[LLI_WORD0_OFFSET] = (addr & U32_MAX);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	lli_p[LLI_WORD1_OFFSET] &= ~LLI_HADDR_MASK;
	lli_p[LLI_WORD1_OFFSET] |= FIELD_PREP(LLI_HADDR_MASK, (addr >> 32));
#endif  
}

static inline void cc_lli_set_size(u32 *lli_p, u16 size)
{
	lli_p[LLI_WORD1_OFFSET] &= ~LLI_SIZE_MASK;
	lli_p[LLI_WORD1_OFFSET] |= FIELD_PREP(LLI_SIZE_MASK, size);
}

#endif  
