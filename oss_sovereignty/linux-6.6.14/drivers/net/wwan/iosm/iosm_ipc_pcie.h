#ifndef IOSM_IPC_PCIE_H
#define IOSM_IPC_PCIE_H
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/skbuff.h>
#include "iosm_ipc_irq.h"
#define INTEL_CP_DEVICE_7560_ID 0x7560
#define INTEL_CP_DEVICE_7360_ID 0x7360
#define IPC_DOORBELL_BAR0 0
#define IPC_SCRATCHPAD_BAR2 2
#define IPC_DOORBELL_CH_OFFSET BIT(5)
#define IPC_WRITE_PTR_REG_0 BIT(4)
#define IPC_CAPTURE_PTR_REG_0 BIT(3)
#define IPC_MSI_VECTORS 1
#define IPC_IRQ_VECTORS IPC_MSI_VECTORS
enum ipc_pcie_sleep_state {
	IPC_PCIE_D0L12,
	IPC_PCIE_D3L2,
};
struct iosm_pcie {
	struct pci_dev *pci;
	struct device *dev;
	void __iomem *ipc_regs;
	void __iomem *scratchpad;
	struct iosm_imem *imem;
	int ipc_regs_bar_nr;
	int scratchpad_bar_nr;
	int nvec;
	u32 doorbell_reg_offset;
	u32 doorbell_write;
	u32 doorbell_capture;
	unsigned long suspend;
	enum ipc_pcie_sleep_state d3l2_support;
};
struct ipc_skb_cb {
	dma_addr_t mapping;
	int direction;
	int len;
	u8 op_type;
};
enum ipc_ul_usr_op {
	UL_USR_OP_BLOCKED,
	UL_MUX_OP_ADB,
	UL_DEFAULT,
};
int ipc_pcie_addr_map(struct iosm_pcie *ipc_pcie, unsigned char *data,
		      size_t size, dma_addr_t *mapping, int direction);
void ipc_pcie_addr_unmap(struct iosm_pcie *ipc_pcie, size_t size,
			 dma_addr_t mapping, int direction);
struct sk_buff *ipc_pcie_alloc_skb(struct iosm_pcie *ipc_pcie, size_t size,
				   gfp_t flags, dma_addr_t *mapping,
				   int direction, size_t headroom);
struct sk_buff *ipc_pcie_alloc_local_skb(struct iosm_pcie *ipc_pcie,
					 gfp_t flags, size_t size);
void ipc_pcie_kfree_skb(struct iosm_pcie *ipc_pcie, struct sk_buff *skb);
bool ipc_pcie_check_data_link_active(struct iosm_pcie *ipc_pcie);
int ipc_pcie_suspend(struct iosm_pcie *ipc_pcie);
int ipc_pcie_resume(struct iosm_pcie *ipc_pcie);
bool ipc_pcie_check_aspm_enabled(struct iosm_pcie *ipc_pcie,
				 bool parent);
void ipc_pcie_config_aspm(struct iosm_pcie *ipc_pcie);
#endif
