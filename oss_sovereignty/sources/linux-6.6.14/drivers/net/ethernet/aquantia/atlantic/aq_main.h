




#ifndef AQ_MAIN_H
#define AQ_MAIN_H

#include "aq_common.h"
#include "aq_nic.h"

DECLARE_STATIC_KEY_FALSE(aq_xdp_locking_key);

void aq_ndev_schedule_work(struct work_struct *work);
struct net_device *aq_ndev_alloc(void);
int aq_ndev_open(struct net_device *ndev);
int aq_ndev_close(struct net_device *ndev);

#endif 
