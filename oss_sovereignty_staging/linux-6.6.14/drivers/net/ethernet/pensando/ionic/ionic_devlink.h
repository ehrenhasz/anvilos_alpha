 
 

#ifndef _IONIC_DEVLINK_H_
#define _IONIC_DEVLINK_H_

#include <net/devlink.h>

int ionic_firmware_update(struct ionic_lif *lif, const struct firmware *fw,
			  struct netlink_ext_ack *extack);

struct ionic *ionic_devlink_alloc(struct device *dev);
void ionic_devlink_free(struct ionic *ionic);
int ionic_devlink_register(struct ionic *ionic);
void ionic_devlink_unregister(struct ionic *ionic);

#endif  
