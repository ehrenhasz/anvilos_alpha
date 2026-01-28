


#ifndef _AUX_DRV_H_
#define _AUX_DRV_H_

#include <linux/virtio_pci_modern.h>

#define PDS_VDPA_DRV_DESCRIPTION    "AMD/Pensando vDPA VF Device Driver"
#define PDS_VDPA_DRV_NAME           KBUILD_MODNAME

struct pds_vdpa_aux {
	struct pds_auxiliary_dev *padev;

	struct vdpa_mgmt_dev vdpa_mdev;
	struct pds_vdpa_device *pdsv;

	struct pds_vdpa_ident ident;

	int vf_id;
	struct dentry *dentry;
	struct virtio_pci_modern_device vd_mdev;

	int nintrs;
};
#endif 
