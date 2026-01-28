#ifndef _FNIC_IO_H_
#define _FNIC_IO_H_
#include <scsi/fc/fc_fcp.h>
#define FNIC_DFLT_SG_DESC_CNT  32
#define FNIC_MAX_SG_DESC_CNT        256      
#define FNIC_SG_DESC_ALIGN          16       
struct host_sg_desc {
	__le64 addr;
	__le32 len;
	u32 _resvd;
};
struct fnic_dflt_sgl_list {
	struct host_sg_desc sg_desc[FNIC_DFLT_SG_DESC_CNT];
};
struct fnic_sgl_list {
	struct host_sg_desc sg_desc[FNIC_MAX_SG_DESC_CNT];
};
enum fnic_sgl_list_type {
	FNIC_SGL_CACHE_DFLT = 0,   
	FNIC_SGL_CACHE_MAX,        
	FNIC_SGL_NUM_CACHES        
};
enum fnic_ioreq_state {
	FNIC_IOREQ_NOT_INITED = 0,
	FNIC_IOREQ_CMD_PENDING,
	FNIC_IOREQ_ABTS_PENDING,
	FNIC_IOREQ_ABTS_COMPLETE,
	FNIC_IOREQ_CMD_COMPLETE,
};
struct fnic_io_req {
	struct host_sg_desc *sgl_list;  
	void *sgl_list_alloc;  
	dma_addr_t sense_buf_pa;  
	dma_addr_t sgl_list_pa;	 
	u16 sgl_cnt;
	u8 sgl_type;  
	u8 io_completed:1;  
	u32 port_id;  
	unsigned long start_time;  
	struct completion *abts_done;  
	struct completion *dr_done;  
	unsigned int tag;
	struct scsi_cmnd *sc;  
};
enum fnic_port_speeds {
	DCEM_PORTSPEED_NONE = 0,
	DCEM_PORTSPEED_1G    = 1000,
	DCEM_PORTSPEED_10G   = 10000,
	DCEM_PORTSPEED_20G   = 20000,
	DCEM_PORTSPEED_25G   = 25000,
	DCEM_PORTSPEED_40G   = 40000,
	DCEM_PORTSPEED_4x10G = 41000,
	DCEM_PORTSPEED_100G  = 100000,
};
#endif  
