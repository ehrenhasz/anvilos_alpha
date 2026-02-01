 

#ifndef __FUNETH_DEVLINK_H
#define __FUNETH_DEVLINK_H

#include <net/devlink.h>

struct devlink *fun_devlink_alloc(struct device *dev);
void fun_devlink_free(struct devlink *devlink);
void fun_devlink_register(struct devlink *devlink);
void fun_devlink_unregister(struct devlink *devlink);

#endif  
