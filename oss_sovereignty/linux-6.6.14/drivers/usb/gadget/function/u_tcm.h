#ifndef U_TCM_H
#define U_TCM_H
#include <linux/usb/composite.h>
struct f_tcm_opts {
	struct usb_function_instance	func_inst;
	struct module			*dependent;
	struct mutex			dep_lock;
	bool				ready;
	bool				can_attach;
	bool				has_dep;
	int (*tcm_register_callback)(struct usb_function_instance *);
	void (*tcm_unregister_callback)(struct usb_function_instance *);
};
#endif  
