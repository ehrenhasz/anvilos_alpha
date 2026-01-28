




#ifndef _SELINUX_NETPORT_H
#define _SELINUX_NETPORT_H

#include <linux/types.h>

void sel_netport_flush(void);

int sel_netport_sid(u8 protocol, u16 pnum, u32 *sid);

#endif
