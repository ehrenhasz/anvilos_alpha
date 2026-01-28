


#ifndef _MEI_INTERFACE_H_
#define _MEI_INTERFACE_H_

#include <linux/irqreturn.h>
#include <linux/pci.h>
#include <linux/mei.h>

#include "mei_dev.h"
#include "client.h"


struct mei_cfg {
	const struct mei_fw_status fw_status;
	bool (*quirk_probe)(const struct pci_dev *pdev);
	const char *kind;
	size_t dma_size[DMA_DSCR_NUM];
	u32 fw_ver_supported:1;
	u32 hw_trc_supported:1;
};


#define MEI_PCI_DEVICE(dev, cfg) \
	.vendor = PCI_VENDOR_ID_INTEL, .device = (dev), \
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID, \
	.driver_data = (kernel_ulong_t)(cfg),

#define MEI_ME_RPM_TIMEOUT    500 


struct mei_me_hw {
	const struct mei_cfg *cfg;
	void __iomem *mem_addr;
	int irq;
	enum mei_pg_state pg_state;
	bool d0i3_supported;
	u8 hbuf_depth;
	int (*read_fws)(const struct mei_device *dev, int where, u32 *val);
	
	struct task_struct *polling_thread;
	wait_queue_head_t wait_active;
	bool is_active;
};

#define to_me_hw(dev) (struct mei_me_hw *)((dev)->hw)

static inline bool mei_me_hw_use_polling(const struct mei_me_hw *hw)
{
	return hw->irq < 0;
}


enum mei_cfg_idx {
	MEI_ME_UNDEF_CFG,
	MEI_ME_ICH_CFG,
	MEI_ME_ICH10_CFG,
	MEI_ME_PCH6_CFG,
	MEI_ME_PCH7_CFG,
	MEI_ME_PCH_CPT_PBG_CFG,
	MEI_ME_PCH8_CFG,
	MEI_ME_PCH8_ITOUCH_CFG,
	MEI_ME_PCH8_SPS_4_CFG,
	MEI_ME_PCH12_CFG,
	MEI_ME_PCH12_SPS_4_CFG,
	MEI_ME_PCH12_SPS_CFG,
	MEI_ME_PCH12_SPS_ITOUCH_CFG,
	MEI_ME_PCH15_CFG,
	MEI_ME_PCH15_SPS_CFG,
	MEI_ME_GSC_CFG,
	MEI_ME_GSCFI_CFG,
	MEI_ME_NUM_CFG,
};

const struct mei_cfg *mei_me_get_cfg(kernel_ulong_t idx);

struct mei_device *mei_me_dev_init(struct device *parent,
				   const struct mei_cfg *cfg, bool slow_fw);

int mei_me_pg_enter_sync(struct mei_device *dev);
int mei_me_pg_exit_sync(struct mei_device *dev);

irqreturn_t mei_me_irq_quick_handler(int irq, void *dev_id);
irqreturn_t mei_me_irq_thread_handler(int irq, void *dev_id);
int mei_me_polling_thread(void *_dev);

#endif 
