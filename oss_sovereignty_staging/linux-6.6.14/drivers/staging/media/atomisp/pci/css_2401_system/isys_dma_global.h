 
 

#ifndef __ISYS_DMA_GLOBAL_H_INCLUDED__
#define __ISYS_DMA_GLOBAL_H_INCLUDED__

#include <type_support.h>

#define HIVE_ISYS2401_DMA_IBUF_DDR_CONN	0
#define HIVE_ISYS2401_DMA_IBUF_VMEM_CONN	1
#define _DMA_V2_ZERO_EXTEND		0
#define _DMA_V2_SIGN_EXTEND		1

#define _DMA_ZERO_EXTEND     _DMA_V2_ZERO_EXTEND
#define _DMA_SIGN_EXTEND     _DMA_V2_SIGN_EXTEND

 
typedef struct isys2401_dma_port_cfg_s isys2401_dma_port_cfg_t;
struct isys2401_dma_port_cfg_s {
	u32 stride;
	u32 elements;
	u32 cropping;
	u32 width;
};

 

 
typedef enum {
	isys2401_dma_ibuf_to_ddr_connection	= HIVE_ISYS2401_DMA_IBUF_DDR_CONN,
	isys2401_dma_ibuf_to_vmem_connection	= HIVE_ISYS2401_DMA_IBUF_VMEM_CONN
} isys2401_dma_connection;

typedef enum {
	isys2401_dma_zero_extension = _DMA_ZERO_EXTEND,
	isys2401_dma_sign_extension = _DMA_SIGN_EXTEND
} isys2401_dma_extension;

typedef struct isys2401_dma_cfg_s isys2401_dma_cfg_t;
struct isys2401_dma_cfg_s {
	isys2401_dma_channel	channel;
	isys2401_dma_connection	connection;
	isys2401_dma_extension	extension;
	u32		height;
};

 

 
extern const isys2401_dma_channel
N_ISYS2401_DMA_CHANNEL_PROCS[N_ISYS2401_DMA_ID];

#endif  
