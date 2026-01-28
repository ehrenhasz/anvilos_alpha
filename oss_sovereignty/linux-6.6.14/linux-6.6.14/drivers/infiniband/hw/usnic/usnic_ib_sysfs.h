#ifndef USNIC_IB_SYSFS_H_
#define USNIC_IB_SYSFS_H_
#include "usnic_ib.h"
int usnic_ib_sysfs_register_usdev(struct usnic_ib_dev *us_ibdev);
void usnic_ib_sysfs_unregister_usdev(struct usnic_ib_dev *us_ibdev);
void usnic_ib_sysfs_qpn_add(struct usnic_ib_qp_grp *qp_grp);
void usnic_ib_sysfs_qpn_remove(struct usnic_ib_qp_grp *qp_grp);
extern const struct attribute_group usnic_attr_group;
#endif  
