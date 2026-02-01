 
 

#ifndef U_ECM_H
#define U_ECM_H

#include <linux/usb/composite.h>

struct f_ecm_opts {
	struct usb_function_instance	func_inst;
	struct net_device		*net;
	bool				bound;

	 
	struct mutex			lock;
	int				refcnt;
};

#endif  
