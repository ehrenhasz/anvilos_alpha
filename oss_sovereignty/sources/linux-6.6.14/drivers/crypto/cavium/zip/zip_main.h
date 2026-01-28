

#ifndef __ZIP_MAIN_H__
#define __ZIP_MAIN_H__

#include "zip_device.h"
#include "zip_regs.h"


#define PCI_DEVICE_ID_THUNDERX_ZIP   0xA01A


#define PCI_CFG_ZIP_PF_BAR0   0  


#define ZIP_MAX_NUM_QUEUES    8

#define ZIP_128B_ALIGN        7


#define ZIP_CMD_QBUF_SIZE     (8064 + 8)

struct zip_registers {
	char  *reg_name;
	u64   reg_offset;
};


struct zip_stats {
	atomic64_t    comp_req_submit;
	atomic64_t    comp_req_complete;
	atomic64_t    decomp_req_submit;
	atomic64_t    decomp_req_complete;
	atomic64_t    comp_in_bytes;
	atomic64_t    comp_out_bytes;
	atomic64_t    decomp_in_bytes;
	atomic64_t    decomp_out_bytes;
	atomic64_t    decomp_bad_reqs;
};


struct zip_iq {
	u64        *sw_head;
	u64        *sw_tail;
	u64        *hw_tail;
	u64        done_cnt;
	u64        pend_cnt;
	u64        free_flag;

	
	spinlock_t  lock;
};


struct zip_device {
	u32               index;
	void __iomem      *reg_base;
	struct pci_dev    *pdev;

	
	u64               depth;
	u64               onfsize;
	u64               ctxsize;

	struct zip_iq     iq[ZIP_MAX_NUM_QUEUES];
	struct zip_stats  stats;
};


struct zip_device *zip_get_device(int node_id);
int zip_get_node_id(void);
void zip_reg_write(u64 val, u64 __iomem *addr);
u64 zip_reg_read(u64 __iomem *addr);
void zip_update_cmd_bufs(struct zip_device *zip_dev, u32 queue);
u32 zip_load_instr(union zip_inst_s *instr, struct zip_device *zip_dev);

#endif 
