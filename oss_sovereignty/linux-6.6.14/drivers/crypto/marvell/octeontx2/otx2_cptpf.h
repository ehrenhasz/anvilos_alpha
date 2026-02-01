 

#ifndef __OTX2_CPTPF_H
#define __OTX2_CPTPF_H

#include "otx2_cpt_common.h"
#include "otx2_cptpf_ucode.h"
#include "otx2_cptlf.h"

struct otx2_cptpf_dev;
struct otx2_cptvf_info {
	struct otx2_cptpf_dev *cptpf;	 
	struct work_struct vfpf_mbox_work;
	struct pci_dev *vf_dev;
	int vf_id;
	int intr_idx;
};

struct cptpf_flr_work {
	struct work_struct work;
	struct otx2_cptpf_dev *pf;
};

struct otx2_cptpf_dev {
	void __iomem *reg_base;		 
	void __iomem *afpf_mbox_base;	 
	void __iomem *vfpf_mbox_base;    
	struct pci_dev *pdev;		 
	struct otx2_cptvf_info vf[OTX2_CPT_MAX_VFS_NUM];
	struct otx2_cpt_eng_grps eng_grps; 
	struct otx2_cptlfs_info lfs;       
	struct otx2_cptlfs_info cpt1_lfs;  
	 
	union otx2_cpt_eng_caps eng_caps[OTX2_CPT_MAX_ENG_TYPES];
	bool is_eng_caps_discovered;

	 
	struct otx2_mbox	afpf_mbox;
	struct work_struct	afpf_mbox_work;
	struct workqueue_struct *afpf_mbox_wq;

	struct otx2_mbox	afpf_mbox_up;
	struct work_struct	afpf_mbox_up_work;

	 
	struct otx2_mbox	vfpf_mbox;
	struct workqueue_struct *vfpf_mbox_wq;

	struct workqueue_struct	*flr_wq;
	struct cptpf_flr_work   *flr_work;
	struct mutex            lock;    

	unsigned long cap_flag;
	u8 pf_id;                
	u8 max_vfs;		 
	u8 enabled_vfs;		 
	u8 sso_pf_func_ovrd;	 
	u8 kvf_limits;		 
	bool has_cpt1;
	u8 rsrc_req_blkaddr;

	 
	struct devlink *dl;
};

irqreturn_t otx2_cptpf_afpf_mbox_intr(int irq, void *arg);
void otx2_cptpf_afpf_mbox_handler(struct work_struct *work);
void otx2_cptpf_afpf_mbox_up_handler(struct work_struct *work);
irqreturn_t otx2_cptpf_vfpf_mbox_intr(int irq, void *arg);
void otx2_cptpf_vfpf_mbox_handler(struct work_struct *work);

#endif  
