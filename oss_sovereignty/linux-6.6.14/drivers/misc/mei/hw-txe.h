 
 

#ifndef _MEI_HW_TXE_H_
#define _MEI_HW_TXE_H_

#include <linux/irqreturn.h>

#include "hw.h"
#include "hw-txe-regs.h"

#define MEI_TXI_RPM_TIMEOUT    500  

 
#define TXE_INTR_READINESS_BIT  0  
#define TXE_INTR_READINESS      HISR_INT_0_STS
#define TXE_INTR_ALIVENESS_BIT  1  
#define TXE_INTR_ALIVENESS      HISR_INT_1_STS
#define TXE_INTR_OUT_DB_BIT     2  
#define TXE_INTR_OUT_DB         HISR_INT_2_STS
#define TXE_INTR_IN_READY_BIT   8  
#define TXE_INTR_IN_READY       BIT(8)

 
struct mei_txe_hw {
	void __iomem * const *mem_addr;
	u32 aliveness;
	u32 readiness;
	u32 slots;

	wait_queue_head_t wait_aliveness_resp;

	unsigned long intr_cause;
};

#define to_txe_hw(dev) (struct mei_txe_hw *)((dev)->hw)

static inline struct mei_device *hw_txe_to_mei(struct mei_txe_hw *hw)
{
	return container_of((void *)hw, struct mei_device, hw);
}

struct mei_device *mei_txe_dev_init(struct pci_dev *pdev);

irqreturn_t mei_txe_irq_quick_handler(int irq, void *dev_id);
irqreturn_t mei_txe_irq_thread_handler(int irq, void *dev_id);

int mei_txe_aliveness_set_sync(struct mei_device *dev, u32 req);

int mei_txe_setup_satt2(struct mei_device *dev, phys_addr_t addr, u32 range);


#endif  
