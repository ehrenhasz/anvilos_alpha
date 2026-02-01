 
 

#ifndef _VFIO_AP_PRIVATE_H_
#define _VFIO_AP_PRIVATE_H_

#include <linux/types.h>
#include <linux/mdev.h>
#include <linux/delay.h>
#include <linux/eventfd.h>
#include <linux/mutex.h>
#include <linux/kvm_host.h>
#include <linux/vfio.h>
#include <linux/hashtable.h>

#include "ap_bus.h"

#define VFIO_AP_MODULE_NAME "vfio_ap"
#define VFIO_AP_DRV_NAME "vfio_ap"

 
struct ap_matrix_dev {
	struct device device;
	struct ap_config_info info;
	struct list_head mdev_list;
	struct mutex mdevs_lock;  
	struct ap_driver  *vfio_ap_drv;
	struct mutex guests_lock;  
	struct mdev_parent parent;
	struct mdev_type mdev_type;
	struct mdev_type *mdev_types[1];
};

extern struct ap_matrix_dev *matrix_dev;

 
struct ap_matrix {
	unsigned long apm_max;
	DECLARE_BITMAP(apm, 256);
	unsigned long aqm_max;
	DECLARE_BITMAP(aqm, 256);
	unsigned long adm_max;
	DECLARE_BITMAP(adm, 256);
};

 
struct ap_queue_table {
	DECLARE_HASHTABLE(queues, 8);
};

 
struct ap_matrix_mdev {
	struct vfio_device vdev;
	struct list_head node;
	struct ap_matrix matrix;
	struct ap_matrix shadow_apcb;
	struct kvm *kvm;
	crypto_hook pqap_hook;
	struct mdev_device *mdev;
	struct ap_queue_table qtable;
	struct eventfd_ctx *req_trigger;
	DECLARE_BITMAP(apm_add, AP_DEVICES);
	DECLARE_BITMAP(aqm_add, AP_DOMAINS);
	DECLARE_BITMAP(adm_add, AP_DOMAINS);
};

 
struct vfio_ap_queue {
	struct ap_matrix_mdev *matrix_mdev;
	dma_addr_t saved_iova;
	int	apqn;
#define VFIO_AP_ISC_INVALID 0xff
	unsigned char saved_isc;
	struct hlist_node mdev_qnode;
	struct ap_queue_status reset_status;
	struct work_struct reset_work;
};

int vfio_ap_mdev_register(void);
void vfio_ap_mdev_unregister(void);

int vfio_ap_mdev_probe_queue(struct ap_device *queue);
void vfio_ap_mdev_remove_queue(struct ap_device *queue);

int vfio_ap_mdev_resource_in_use(unsigned long *apm, unsigned long *aqm);

void vfio_ap_on_cfg_changed(struct ap_config_info *new_config_info,
			    struct ap_config_info *old_config_info);
void vfio_ap_on_scan_complete(struct ap_config_info *new_config_info,
			      struct ap_config_info *old_config_info);

#endif  
