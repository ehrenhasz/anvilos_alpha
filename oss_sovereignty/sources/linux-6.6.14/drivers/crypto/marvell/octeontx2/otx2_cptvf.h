

#ifndef __OTX2_CPTVF_H
#define __OTX2_CPTVF_H

#include "mbox.h"
#include "otx2_cptlf.h"

struct otx2_cptvf_dev {
	void __iomem *reg_base;		
	void __iomem *pfvf_mbox_base;	
	struct pci_dev *pdev;		
	struct otx2_cptlfs_info lfs;	
	u8 vf_id;			

	
	struct otx2_mbox	pfvf_mbox;
	struct work_struct	pfvf_mbox_work;
	struct workqueue_struct *pfvf_mbox_wq;
	int blkaddr;
	void *bbuf_base;
	unsigned long cap_flag;
};

irqreturn_t otx2_cptvf_pfvf_mbox_intr(int irq, void *arg);
void otx2_cptvf_pfvf_mbox_handler(struct work_struct *work);
int otx2_cptvf_send_eng_grp_num_msg(struct otx2_cptvf_dev *cptvf, int eng_type);
int otx2_cptvf_send_kvf_limits_msg(struct otx2_cptvf_dev *cptvf);
int otx2_cpt_mbox_bbuf_init(struct otx2_cptvf_dev *cptvf, struct pci_dev *pdev);

#endif 
