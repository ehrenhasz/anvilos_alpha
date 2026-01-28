


#ifndef _OCTEP_MAIN_H_
#define _OCTEP_MAIN_H_

#include "octep_tx.h"
#include "octep_rx.h"
#include "octep_ctrl_mbox.h"

#define OCTEP_DRV_NAME		"octeon_ep"
#define OCTEP_DRV_STRING	"Marvell Octeon EndPoint NIC Driver"

#define  OCTEP_PCIID_CN93_PF  0xB200177d
#define  OCTEP_PCIID_CN93_VF  0xB203177d

#define  OCTEP_PCI_DEVICE_ID_CN93_PF 0xB200
#define  OCTEP_PCI_DEVICE_ID_CN93_VF 0xB203

#define  OCTEP_PCI_DEVICE_ID_CNF95N_PF 0xB400    

#define  OCTEP_MAX_QUEUES   63
#define  OCTEP_MAX_IQ       OCTEP_MAX_QUEUES
#define  OCTEP_MAX_OQ       OCTEP_MAX_QUEUES
#define  OCTEP_MAX_VF       64

#define OCTEP_MAX_MSIX_VECTORS OCTEP_MAX_OQ


#define  OCTEP_INPUT_INTR    (1)
#define  OCTEP_OUTPUT_INTR   (2)
#define  OCTEP_MBOX_INTR     (4)
#define  OCTEP_ALL_INTR      0xff

#define  OCTEP_IQ_INTR_RESEND_BIT  59
#define  OCTEP_OQ_INTR_RESEND_BIT  59

#define  OCTEP_MMIO_REGIONS     3

struct octep_mmio {
	
	u8 __iomem *hw_addr;

	
	int mapped;
};

struct octep_pci_win_regs {
	u8 __iomem *pci_win_wr_addr;
	u8 __iomem *pci_win_rd_addr;
	u8 __iomem *pci_win_wr_data;
	u8 __iomem *pci_win_rd_data;
};

struct octep_hw_ops {
	void (*setup_iq_regs)(struct octep_device *oct, int q);
	void (*setup_oq_regs)(struct octep_device *oct, int q);
	void (*setup_mbox_regs)(struct octep_device *oct, int mbox);

	irqreturn_t (*non_ioq_intr_handler)(void *ioq_vector);
	irqreturn_t (*ioq_intr_handler)(void *ioq_vector);
	int (*soft_reset)(struct octep_device *oct);
	void (*reinit_regs)(struct octep_device *oct);
	u32  (*update_iq_read_idx)(struct octep_iq *iq);

	void (*enable_interrupts)(struct octep_device *oct);
	void (*disable_interrupts)(struct octep_device *oct);
	bool (*poll_non_ioq_interrupts)(struct octep_device *oct);

	void (*enable_io_queues)(struct octep_device *oct);
	void (*disable_io_queues)(struct octep_device *oct);
	void (*enable_iq)(struct octep_device *oct, int q);
	void (*disable_iq)(struct octep_device *oct, int q);
	void (*enable_oq)(struct octep_device *oct, int q);
	void (*disable_oq)(struct octep_device *oct, int q);
	void (*reset_io_queues)(struct octep_device *oct);
	void (*dump_registers)(struct octep_device *oct);
};


struct octep_mbox_data {
	u32 cmd;
	u32 total_len;
	u32 recv_len;
	u32 rsvd;
	u64 *data;
};


struct octep_mbox {
	
	spinlock_t lock;

	u32 q_no;
	u32 state;

	
	u8 __iomem *mbox_int_reg;

	
	u8 __iomem *mbox_write_reg;

	
	u8 __iomem *mbox_read_reg;

	struct octep_mbox_data mbox_data;
};


struct octep_ioq_vector {
	char name[OCTEP_MSIX_NAME_SIZE];
	struct napi_struct napi;
	struct octep_device *octep_dev;
	struct octep_iq *iq;
	struct octep_oq *oq;
	cpumask_t affinity_mask;
};


#define OCTEP_CAP_TX_CHECKSUM BIT(0)
#define OCTEP_CAP_RX_CHECKSUM BIT(1)
#define OCTEP_CAP_TSO         BIT(2)


enum octep_link_mode_bit_indices {
	OCTEP_LINK_MODE_10GBASE_T    = 0,
	OCTEP_LINK_MODE_10GBASE_R,
	OCTEP_LINK_MODE_10GBASE_CR,
	OCTEP_LINK_MODE_10GBASE_KR,
	OCTEP_LINK_MODE_10GBASE_LR,
	OCTEP_LINK_MODE_10GBASE_SR,
	OCTEP_LINK_MODE_25GBASE_CR,
	OCTEP_LINK_MODE_25GBASE_KR,
	OCTEP_LINK_MODE_25GBASE_SR,
	OCTEP_LINK_MODE_40GBASE_CR4,
	OCTEP_LINK_MODE_40GBASE_KR4,
	OCTEP_LINK_MODE_40GBASE_LR4,
	OCTEP_LINK_MODE_40GBASE_SR4,
	OCTEP_LINK_MODE_50GBASE_CR2,
	OCTEP_LINK_MODE_50GBASE_KR2,
	OCTEP_LINK_MODE_50GBASE_SR2,
	OCTEP_LINK_MODE_50GBASE_CR,
	OCTEP_LINK_MODE_50GBASE_KR,
	OCTEP_LINK_MODE_50GBASE_LR,
	OCTEP_LINK_MODE_50GBASE_SR,
	OCTEP_LINK_MODE_100GBASE_CR4,
	OCTEP_LINK_MODE_100GBASE_KR4,
	OCTEP_LINK_MODE_100GBASE_LR4,
	OCTEP_LINK_MODE_100GBASE_SR4,
	OCTEP_LINK_MODE_NBITS
};


struct octep_iface_link_info {
	
	u64 supported_modes;

	
	u64 advertised_modes;

	
	u32 speed;

	
	u16 mtu;

	
#define OCTEP_LINK_MODE_AUTONEG_SUPPORTED   BIT(0)
#define OCTEP_LINK_MODE_AUTONEG_ADVERTISED  BIT(1)
	u8 autoneg;

	
#define OCTEP_LINK_MODE_PAUSE_SUPPORTED   BIT(0)
#define OCTEP_LINK_MODE_PAUSE_ADVERTISED  BIT(1)
	u8 pause;

	
	u8  admin_up;

	
	u8  oper_up;
};


struct octep_device {
	struct octep_config *conf;

	
	u16 chip_id;
	u16 rev_id;

	
	u64 caps_enabled;
	
	u64 caps_supported;

	
	struct device *dev;
	
	struct pci_dev *pdev;
	
	struct net_device *netdev;

	
	struct octep_mmio mmio[OCTEP_MMIO_REGIONS];

	
	u8 mac_addr[ETH_ALEN];

	
	u16 num_iqs;
	
	u8 pkind;
	
	struct octep_iq *iq[OCTEP_MAX_IQ];

	
	u16 num_oqs;
	
	struct octep_oq *oq[OCTEP_MAX_OQ];

	
	u16 pcie_port;

	
	struct octep_pci_win_regs pci_win_regs;
	
	struct octep_hw_ops hw_ops;

	
	u16 num_irqs;
	u16 num_non_ioq_irqs;
	char *non_ioq_irq_names;
	struct msix_entry *msix_entries;
	
	struct octep_ioq_vector *ioq_vector[OCTEP_MAX_QUEUES];

	
	struct octep_iface_tx_stats iface_tx_stats;
	
	struct octep_iface_rx_stats iface_rx_stats;

	
	struct octep_iface_link_info link_info;

	
	struct octep_mbox *mbox[OCTEP_MAX_VF];

	
	struct work_struct tx_timeout_task;

	
	struct octep_ctrl_mbox ctrl_mbox;

	
	u32 ctrl_mbox_ifstats_offset;

	
	struct work_struct ctrl_mbox_task;
	
	wait_queue_head_t ctrl_req_wait_q;
	
	struct list_head ctrl_req_wait_list;

	
	bool poll_non_ioq_intr;
	
	struct delayed_work intr_poll_task;

	
	struct timer_list hb_timer;
	
	atomic_t hb_miss_cnt;
	
	struct delayed_work hb_task;
};

static inline u16 OCTEP_MAJOR_REV(struct octep_device *oct)
{
	u16 rev = (oct->rev_id & 0xC) >> 2;

	return (rev == 0) ? 1 : rev;
}

static inline u16 OCTEP_MINOR_REV(struct octep_device *oct)
{
	return (oct->rev_id & 0x3);
}


#define octep_write_csr(octep_dev, reg_off, value) \
	writel(value, (octep_dev)->mmio[0].hw_addr + (reg_off))

#define octep_write_csr64(octep_dev, reg_off, val64) \
	writeq(val64, (octep_dev)->mmio[0].hw_addr + (reg_off))

#define octep_read_csr(octep_dev, reg_off)         \
	readl((octep_dev)->mmio[0].hw_addr + (reg_off))

#define octep_read_csr64(octep_dev, reg_off)         \
	readq((octep_dev)->mmio[0].hw_addr + (reg_off))


static inline u64
OCTEP_PCI_WIN_READ(struct octep_device *oct, u64 addr)
{
	u64 val64;

	addr |= 1ull << 53; 
	writeq(addr, oct->pci_win_regs.pci_win_rd_addr);
	val64 = readq(oct->pci_win_regs.pci_win_rd_data);

	dev_dbg(&oct->pdev->dev,
		"%s: reg: 0x%016llx val: 0x%016llx\n", __func__, addr, val64);

	return val64;
}


static inline void
OCTEP_PCI_WIN_WRITE(struct octep_device *oct, u64 addr, u64 val)
{
	writeq(addr, oct->pci_win_regs.pci_win_wr_addr);
	writeq(val, oct->pci_win_regs.pci_win_wr_data);

	dev_dbg(&oct->pdev->dev,
		"%s: reg: 0x%016llx val: 0x%016llx\n", __func__, addr, val);
}

extern struct workqueue_struct *octep_wq;

int octep_device_setup(struct octep_device *oct);
int octep_setup_iqs(struct octep_device *oct);
void octep_free_iqs(struct octep_device *oct);
void octep_clean_iqs(struct octep_device *oct);
int octep_setup_oqs(struct octep_device *oct);
void octep_free_oqs(struct octep_device *oct);
void octep_oq_dbell_init(struct octep_device *oct);
void octep_device_setup_cn93_pf(struct octep_device *oct);
int octep_iq_process_completions(struct octep_iq *iq, u16 budget);
int octep_oq_process_rx(struct octep_oq *oq, int budget);
void octep_set_ethtool_ops(struct net_device *netdev);

#endif 
