 
 

#ifndef U_PRINTER_H
#define U_PRINTER_H

#include <linux/usb/composite.h>

struct f_printer_opts {
	struct usb_function_instance	func_inst;
	int				minor;
	char				*pnp_string;
	bool				pnp_string_allocated;
	unsigned			q_len;

	 
	struct mutex			lock;
	int				refcnt;
};

#endif  
