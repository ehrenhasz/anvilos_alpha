




#ifndef __TEE_DEV_H__
#define __TEE_DEV_H__

#include <linux/device.h>
#include <linux/mutex.h>

#define TEE_DEFAULT_TIMEOUT		10
#define MAX_BUFFER_SIZE			988


enum tee_ring_cmd_id {
	TEE_RING_INIT_CMD		= 0x00010000,
	TEE_RING_DESTROY_CMD		= 0x00020000,
	TEE_RING_MAX_CMD		= 0x000F0000,
};


struct tee_init_ring_cmd {
	u32 low_addr;
	u32 hi_addr;
	u32 size;
};

#define MAX_RING_BUFFER_ENTRIES		32


struct ring_buf_manager {
	struct mutex mutex;	
	void *ring_start;
	u32 ring_size;
	phys_addr_t ring_pa;
	u32 wptr;
};

struct psp_tee_device {
	struct device *dev;
	struct psp_device *psp;
	void __iomem *io_regs;
	struct tee_vdata *vdata;
	struct ring_buf_manager rb_mgr;
};


enum tee_cmd_state {
	TEE_CMD_STATE_INIT,
	TEE_CMD_STATE_PROCESS,
	TEE_CMD_STATE_COMPLETED,
};


enum cmd_resp_state {
	CMD_RESPONSE_INVALID,
	CMD_WAITING_FOR_RESPONSE,
	CMD_RESPONSE_TIMEDOUT,
	CMD_RESPONSE_COPIED,
};


struct tee_ring_cmd {
	u32 cmd_id;
	u32 cmd_state;
	u32 status;
	u32 res0[1];
	u64 pdata;
	u32 res1[2];
	u8 buf[MAX_BUFFER_SIZE];
	u32 flag;

	
} __packed;

int tee_dev_init(struct psp_device *psp);
void tee_dev_destroy(struct psp_device *psp);

#endif 
