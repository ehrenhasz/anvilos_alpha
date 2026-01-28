


#ifndef _PRESTERA_DEVLINK_H_
#define _PRESTERA_DEVLINK_H_

#include "prestera.h"

struct prestera_switch *prestera_devlink_alloc(struct prestera_device *dev);
void prestera_devlink_free(struct prestera_switch *sw);

void prestera_devlink_register(struct prestera_switch *sw);
void prestera_devlink_unregister(struct prestera_switch *sw);

int prestera_devlink_port_register(struct prestera_port *port);
void prestera_devlink_port_unregister(struct prestera_port *port);

void prestera_devlink_trap_report(struct prestera_port *port,
				  struct sk_buff *skb, u8 cpu_code);
int prestera_devlink_traps_register(struct prestera_switch *sw);
void prestera_devlink_traps_unregister(struct prestera_switch *sw);

#endif 
