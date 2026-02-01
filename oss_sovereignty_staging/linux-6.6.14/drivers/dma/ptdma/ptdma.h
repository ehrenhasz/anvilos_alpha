 
 

#ifndef __PT_DEV_H__
#define __PT_DEV_H__

#include <linux/device.h>
#include <linux/dmaengine.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/dmapool.h>

#include "../virt-dma.h"

#define MAX_PT_NAME_LEN			16
#define MAX_DMAPOOL_NAME_LEN		32

#define MAX_HW_QUEUES			1
#define MAX_CMD_QLEN			100

#define PT_ENGINE_PASSTHRU		5

 
#define IRQ_MASK_REG			0x040
#define IRQ_STATUS_REG			0x200

#define CMD_Q_ERROR(__qs)		((__qs) & 0x0000003f)

#define CMD_QUEUE_PRIO_OFFSET		0x00
#define CMD_REQID_CONFIG_OFFSET		0x04
#define CMD_TIMEOUT_OFFSET		0x08
#define CMD_PT_VERSION			0x10

#define CMD_Q_CONTROL_BASE		0x0000
#define CMD_Q_TAIL_LO_BASE		0x0004
#define CMD_Q_HEAD_LO_BASE		0x0008
#define CMD_Q_INT_ENABLE_BASE		0x000C
#define CMD_Q_INTERRUPT_STATUS_BASE	0x0010

#define CMD_Q_STATUS_BASE		0x0100
#define CMD_Q_INT_STATUS_BASE		0x0104
#define CMD_Q_DMA_STATUS_BASE		0x0108
#define CMD_Q_DMA_READ_STATUS_BASE	0x010C
#define CMD_Q_DMA_WRITE_STATUS_BASE	0x0110
#define CMD_Q_ABORT_BASE		0x0114
#define CMD_Q_AX_CACHE_BASE		0x0118

#define CMD_CONFIG_OFFSET		0x1120
#define CMD_CLK_GATE_CTL_OFFSET		0x6004

#define CMD_DESC_DW0_VAL		0x500012

 
#define CMD_Q_STATUS_INCR		0x1000

 
#define CMD_CONFIG_REQID		0
#define CMD_TIMEOUT_DISABLE		0
#define CMD_CLK_DYN_GATING_DIS		0
#define CMD_CLK_SW_GATE_MODE		0
#define CMD_CLK_GATE_CTL		0
#define CMD_QUEUE_PRIO			GENMASK(2, 1)
#define CMD_CONFIG_VHB_EN		BIT(0)
#define CMD_CLK_DYN_GATING_EN		BIT(0)
#define CMD_CLK_HW_GATE_MODE		BIT(0)
#define CMD_CLK_GATE_ON_DELAY		BIT(12)
#define CMD_CLK_GATE_OFF_DELAY		BIT(12)

#define CMD_CLK_GATE_CONFIG		(CMD_CLK_GATE_CTL | \
					CMD_CLK_HW_GATE_MODE | \
					CMD_CLK_GATE_ON_DELAY | \
					CMD_CLK_DYN_GATING_EN | \
					CMD_CLK_GATE_OFF_DELAY)

#define CMD_Q_LEN			32
#define CMD_Q_RUN			BIT(0)
#define CMD_Q_HALT			BIT(1)
#define CMD_Q_MEM_LOCATION		BIT(2)
#define CMD_Q_SIZE_MASK			GENMASK(4, 0)
#define CMD_Q_SIZE			GENMASK(7, 3)
#define CMD_Q_SHIFT			GENMASK(1, 0)
#define QUEUE_SIZE_VAL			((ffs(CMD_Q_LEN) - 2) & \
								  CMD_Q_SIZE_MASK)
#define Q_PTR_MASK			(2 << (QUEUE_SIZE_VAL + 5) - 1)
#define Q_DESC_SIZE			sizeof(struct ptdma_desc)
#define Q_SIZE(n)			(CMD_Q_LEN * (n))

#define INT_COMPLETION			BIT(0)
#define INT_ERROR			BIT(1)
#define INT_QUEUE_STOPPED		BIT(2)
#define INT_EMPTY_QUEUE			BIT(3)
#define SUPPORTED_INTERRUPTS		(INT_COMPLETION | INT_ERROR)

 
#define LSB_START			0
#define LSB_END				127
#define LSB_COUNT			(LSB_END - LSB_START + 1)

#define PT_DMAPOOL_MAX_SIZE		64
#define PT_DMAPOOL_ALIGN		BIT(5)

#define PT_PASSTHRU_BLOCKSIZE		512

struct pt_device;

struct pt_tasklet_data {
	struct completion completion;
	struct pt_cmd *cmd;
};

 
struct pt_passthru_engine {
	dma_addr_t mask;
	u32 mask_len;		 

	dma_addr_t src_dma, dst_dma;
	u64 src_len;		 
};

 
struct pt_cmd {
	struct list_head entry;
	struct work_struct work;
	struct pt_device *pt;
	int ret;
	u32 engine;
	u32 engine_error;
	struct pt_passthru_engine passthru;
	 
	void (*pt_cmd_callback)(void *data, int err);
	void *data;
};

struct pt_dma_desc {
	struct virt_dma_desc vd;
	struct pt_device *pt;
	enum dma_status status;
	size_t len;
	bool issued_to_hw;
	struct pt_cmd pt_cmd;
};

struct pt_dma_chan {
	struct virt_dma_chan vc;
	struct pt_device *pt;
};

struct pt_cmd_queue {
	struct pt_device *pt;

	 
	struct dma_pool *dma_pool;

	 
	struct ptdma_desc *qbase;

	 
	spinlock_t q_lock ____cacheline_aligned;
	unsigned int qidx;

	unsigned int qsize;
	dma_addr_t qbase_dma;
	dma_addr_t qdma_tail;

	unsigned int active;
	unsigned int suspended;

	 
	bool int_en;

	 
	void __iomem *reg_control;
	u32 qcontrol;  

	 
	u32 int_status;
	u32 q_status;
	u32 q_int_status;
	u32 cmd_error;
	 
	unsigned long total_pt_ops;
} ____cacheline_aligned;

struct pt_device {
	struct list_head entry;

	unsigned int ord;
	char name[MAX_PT_NAME_LEN];

	struct device *dev;

	 
	struct pt_msix *pt_msix;

	struct pt_dev_vdata *dev_vdata;

	unsigned int pt_irq;

	 
	void __iomem *io_regs;

	spinlock_t cmd_lock ____cacheline_aligned;
	unsigned int cmd_count;
	struct list_head cmd;

	 
	struct pt_cmd_queue cmd_q;

	 
	struct dma_device dma_dev;
	struct pt_dma_chan *pt_dma_chan;
	struct kmem_cache *dma_cmd_cache;
	struct kmem_cache *dma_desc_cache;

	wait_queue_head_t lsb_queue;

	 
	unsigned long total_interrupts;

	struct pt_tasklet_data tdata;
};

 

#define DWORD0_SOC	BIT(0)
#define DWORD0_IOC	BIT(1)

struct dword3 {
	unsigned int  src_hi:16;
	unsigned int  src_mem:2;
	unsigned int  lsb_cxt_id:8;
	unsigned int  rsvd1:5;
	unsigned int  fixed:1;
};

struct dword5 {
	unsigned int  dst_hi:16;
	unsigned int  dst_mem:2;
	unsigned int  rsvd1:13;
	unsigned int  fixed:1;
};

struct ptdma_desc {
	u32 dw0;
	u32 length;
	u32 src_lo;
	struct dword3 dw3;
	u32 dst_lo;
	struct dword5 dw5;
	__le32 rsvd1;
	__le32 rsvd2;
};

 
struct pt_dev_vdata {
	const unsigned int bar;
};

int pt_dmaengine_register(struct pt_device *pt);
void pt_dmaengine_unregister(struct pt_device *pt);

void ptdma_debugfs_setup(struct pt_device *pt);
int pt_core_init(struct pt_device *pt);
void pt_core_destroy(struct pt_device *pt);

int pt_core_perform_passthru(struct pt_cmd_queue *cmd_q,
			     struct pt_passthru_engine *pt_engine);

void pt_check_status_trans(struct pt_device *pt, struct pt_cmd_queue *cmd_q);
void pt_start_queue(struct pt_cmd_queue *cmd_q);
void pt_stop_queue(struct pt_cmd_queue *cmd_q);

static inline void pt_core_disable_queue_interrupts(struct pt_device *pt)
{
	iowrite32(0, pt->cmd_q.reg_control + 0x000C);
}

static inline void pt_core_enable_queue_interrupts(struct pt_device *pt)
{
	iowrite32(SUPPORTED_INTERRUPTS, pt->cmd_q.reg_control + 0x000C);
}
#endif
