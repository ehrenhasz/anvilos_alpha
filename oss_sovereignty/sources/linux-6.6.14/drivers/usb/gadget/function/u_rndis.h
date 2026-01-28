


#ifndef U_RNDIS_H
#define U_RNDIS_H

#include <linux/usb/composite.h>

struct f_rndis_opts {
	struct usb_function_instance	func_inst;
	u32				vendor_id;
	const char			*manufacturer;
	struct net_device		*net;
	bool				bound;
	bool				borrowed_net;

	struct config_group		*rndis_interf_group;
	struct usb_os_desc		rndis_os_desc;
	char				rndis_ext_compat_id[16];

	u8				class;
	u8				subclass;
	u8				protocol;

	
	struct mutex			lock;
	int				refcnt;
};

void rndis_borrow_net(struct usb_function_instance *f, struct net_device *net);

#endif 
