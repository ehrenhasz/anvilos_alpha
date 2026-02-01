 
 

#ifndef _PRESTERA_SWITCHDEV_H_
#define _PRESTERA_SWITCHDEV_H_

int prestera_switchdev_init(struct prestera_switch *sw);
void prestera_switchdev_fini(struct prestera_switch *sw);

int prestera_bridge_port_join(struct net_device *br_dev,
			      struct prestera_port *port,
			      struct netlink_ext_ack *extack);

void prestera_bridge_port_leave(struct net_device *br_dev,
				struct prestera_port *port);

#endif  
