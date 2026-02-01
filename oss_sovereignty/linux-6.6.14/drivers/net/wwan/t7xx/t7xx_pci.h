 

#ifndef __T7XX_PCI_H__
#define __T7XX_PCI_H__

#include <linux/completion.h>
#include <linux/irqreturn.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include "t7xx_reg.h"

 
struct t7xx_addr_base {
	void __iomem		*pcie_mac_ireg_base;
	void __iomem		*pcie_ext_reg_base;
	u32			pcie_dev_reg_trsl_addr;
	void __iomem		*infracfg_ao_base;
	void __iomem		*mhccif_rc_base;
};

typedef irqreturn_t (*t7xx_intr_callback)(int irq, void *param);

 
struct t7xx_pci_dev {
	t7xx_intr_callback	intr_handler[EXT_INT_NUM];
	t7xx_intr_callback	intr_thread[EXT_INT_NUM];
	void			*callback_param[EXT_INT_NUM];
	struct pci_dev		*pdev;
	struct t7xx_addr_base	base_addr;
	struct t7xx_modem	*md;
	struct t7xx_ccmni_ctrl	*ccmni_ctlb;
	bool			rgu_pci_irq_en;
	struct completion	init_done;

	 
	struct list_head	md_pm_entities;
	struct mutex		md_pm_entity_mtx;	 
	struct completion	pm_sr_ack;
	atomic_t		md_pm_state;
	spinlock_t		md_pm_lock;		 
	unsigned int		sleep_disable_count;
	struct completion	sleep_lock_acquire;
#ifdef CONFIG_WWAN_DEBUGFS
	struct dentry		*debugfs_dir;
#endif
};

enum t7xx_pm_id {
	PM_ENTITY_ID_CTRL1,
	PM_ENTITY_ID_CTRL2,
	PM_ENTITY_ID_DATA,
	PM_ENTITY_ID_INVALID
};

 
struct md_pm_entity {
	struct list_head	entity;
	int (*suspend)(struct t7xx_pci_dev *t7xx_dev, void *entity_param);
	void (*suspend_late)(struct t7xx_pci_dev *t7xx_dev, void *entity_param);
	void (*resume_early)(struct t7xx_pci_dev *t7xx_dev, void *entity_param);
	int (*resume)(struct t7xx_pci_dev *t7xx_dev, void *entity_param);
	enum t7xx_pm_id		id;
	void			*entity_param;
};

void t7xx_pci_disable_sleep(struct t7xx_pci_dev *t7xx_dev);
void t7xx_pci_enable_sleep(struct t7xx_pci_dev *t7xx_dev);
int t7xx_pci_sleep_disable_complete(struct t7xx_pci_dev *t7xx_dev);
int t7xx_pci_pm_entity_register(struct t7xx_pci_dev *t7xx_dev, struct md_pm_entity *pm_entity);
int t7xx_pci_pm_entity_unregister(struct t7xx_pci_dev *t7xx_dev, struct md_pm_entity *pm_entity);
void t7xx_pci_pm_init_late(struct t7xx_pci_dev *t7xx_dev);
void t7xx_pci_pm_exp_detected(struct t7xx_pci_dev *t7xx_dev);

#endif  
