 
 

#ifndef _cell_params_h
#define _cell_params_h

#define SP_PMEM_LOG_WIDTH_BITS           6   
#define SP_ICACHE_TAG_BITS               4   
#define SP_ICACHE_SET_BITS               8   
#define SP_ICACHE_BLOCKS_PER_SET_BITS    1   
#define SP_ICACHE_BLOCK_ADDRESS_BITS     11  

#define SP_ICACHE_ADDRESS_BITS \
			    (SP_ICACHE_TAG_BITS + SP_ICACHE_BLOCK_ADDRESS_BITS)

#define SP_PMEM_DEPTH        BIT(SP_ICACHE_ADDRESS_BITS)

#define SP_FIFO_0_DEPTH      0
#define SP_FIFO_1_DEPTH      0
#define SP_FIFO_2_DEPTH      0
#define SP_FIFO_3_DEPTH      0
#define SP_FIFO_4_DEPTH      0
#define SP_FIFO_5_DEPTH      0
#define SP_FIFO_6_DEPTH      0
#define SP_FIFO_7_DEPTH      0

#define SP_SLV_BUS_MAXBURSTSIZE        1

#endif  
