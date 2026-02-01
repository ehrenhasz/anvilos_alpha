 
#ifndef USNIC_DEBUGFS_H_
#define USNIC_DEBUGFS_H_

#include "usnic_ib_qp_grp.h"

void usnic_debugfs_init(void);

void usnic_debugfs_exit(void);
void usnic_debugfs_flow_add(struct usnic_ib_qp_grp_flow *qp_flow);
void usnic_debugfs_flow_remove(struct usnic_ib_qp_grp_flow *qp_flow);

#endif  
