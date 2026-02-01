 

#ifndef __OTX_CPTPF_H
#define __OTX_CPTPF_H

#include <linux/types.h>
#include <linux/device.h>
#include "otx_cptpf_ucode.h"

 
struct otx_cpt_device {
	void __iomem *reg_base;  
	struct pci_dev *pdev;	 
	struct otx_cpt_eng_grps eng_grps; 
	struct list_head list;
	u8 pf_type;	 
	u8 max_vfs;	 
	u8 vfs_enabled;	 
};

void otx_cpt_mbox_intr_handler(struct otx_cpt_device *cpt, int mbx);
void otx_cpt_disable_all_cores(struct otx_cpt_device *cpt);

#endif  
