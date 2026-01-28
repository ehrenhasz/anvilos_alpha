




#ifndef _SELINUX_NETNODE_H
#define _SELINUX_NETNODE_H

#include <linux/types.h>

void sel_netnode_flush(void);

int sel_netnode_sid(void *addr, u16 family, u32 *sid);

#endif
