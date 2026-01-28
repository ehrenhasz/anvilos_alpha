


#ifndef U_EEM_H
#define U_EEM_H

#include <linux/usb/composite.h>

struct f_eem_opts {
	struct usb_function_instance	func_inst;
	struct net_device		*net;
	bool				bound;

	
	struct mutex			lock;
	int				refcnt;
};

#endif 
