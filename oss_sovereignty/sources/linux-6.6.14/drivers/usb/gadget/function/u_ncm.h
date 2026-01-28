


#ifndef U_NCM_H
#define U_NCM_H

#include <linux/usb/composite.h>

struct f_ncm_opts {
	struct usb_function_instance	func_inst;
	struct net_device		*net;
	bool				bound;

	struct config_group		*ncm_interf_group;
	struct usb_os_desc		ncm_os_desc;
	char				ncm_ext_compat_id[16];
	
	struct mutex			lock;
	int				refcnt;
};

#endif 
