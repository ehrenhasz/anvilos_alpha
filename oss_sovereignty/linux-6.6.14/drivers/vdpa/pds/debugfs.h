#ifndef _PDS_VDPA_DEBUGFS_H_
#define _PDS_VDPA_DEBUGFS_H_
#include <linux/debugfs.h>
void pds_vdpa_debugfs_create(void);
void pds_vdpa_debugfs_destroy(void);
void pds_vdpa_debugfs_add_pcidev(struct pds_vdpa_aux *vdpa_aux);
void pds_vdpa_debugfs_add_ident(struct pds_vdpa_aux *vdpa_aux);
void pds_vdpa_debugfs_add_vdpadev(struct pds_vdpa_aux *vdpa_aux);
void pds_vdpa_debugfs_del_vdpadev(struct pds_vdpa_aux *vdpa_aux);
void pds_vdpa_debugfs_reset_vdpadev(struct pds_vdpa_aux *vdpa_aux);
#endif  
