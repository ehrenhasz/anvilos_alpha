 
 

#ifndef U_MIDI_H
#define U_MIDI_H

#include <linux/usb/composite.h>

struct f_midi_opts {
	struct usb_function_instance	func_inst;
	int				index;
	char				*id;
	bool				id_allocated;
	unsigned int			in_ports;
	unsigned int			out_ports;
	unsigned int			buflen;
	unsigned int			qlen;

	 
	struct mutex			lock;
	int				refcnt;
};

#endif  

