#ifndef __IBUF_CTRL_GLOBAL_H_INCLUDED__
#define __IBUF_CTRL_GLOBAL_H_INCLUDED__
#include <type_support.h>
#include <ibuf_cntrl_defs.h>	 
#define _IBUF_CNTRL_MAIN_CNTRL_FSM_MASK			0xf
#define _IBUF_CNTRL_MAIN_CNTRL_FSM_NEXT_COMMAND_CHECK	0x9
#define _IBUF_CNTRL_MAIN_CNTRL_MEM_INP_BUF_ALLOC	BIT(8)
#define _IBUF_CNTRL_DMA_SYNC_WAIT_FOR_SYNC		1
#define _IBUF_CNTRL_DMA_SYNC_FSM_WAIT_FOR_ACK		(0x3 << 1)
struct	isp2401_ib_buffer_s {
	u32	start_addr;	 
	u32	stride;		 
	u32	lines;		 
};
typedef struct isp2401_ib_buffer_s	isp2401_ib_buffer_t;
typedef struct ibuf_ctrl_cfg_s ibuf_ctrl_cfg_t;
struct ibuf_ctrl_cfg_s {
	bool online;
	struct {
		u32 channel;
		u32 cmd;  
		u32 shift_returned_items;
		u32 elems_per_word_in_ibuf;
		u32 elems_per_word_in_dest;
	} dma_cfg;
	isp2401_ib_buffer_t ib_buffer;
	struct {
		u32 stride;
		u32 start_addr;
		u32 lines;
	} dest_buf_cfg;
	u32 items_per_store;
	u32 stores_per_frame;
	struct {
		u32 sync_cmd;	 
		u32 store_cmd;	 
	} stream2mmio_cfg;
};
extern const u32 N_IBUF_CTRL_PROCS[N_IBUF_CTRL_ID];
#endif  
